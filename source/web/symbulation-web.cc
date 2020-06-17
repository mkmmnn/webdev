#include <iostream>
#include "../SymWorld.h"
#include "../../Empirical/source/config/ArgManager.h"
#include "../Web.h"

using namespace std;

EMP_BUILD_CONFIG(SymConfigBase,
    VALUE(SEED, int, 10, "What value should the random seed be? If seed <= 0, then it is randomly re-chosen."),
    VALUE(DATA_INT, int, 100, "How frequently, in updates, should data print?"),
    VALUE(MUTATION_RATE, double, 0.002, "Standard deviation of the distribution to mutate by"),
    VALUE(SYNERGY, double, 5, "Amount symbiont's returned resources should be multiplied by"),
    VALUE(VERTICAL_TRANSMISSION, double, 1, "Value 0 to 1 of probability of symbiont vertically transmitting when host reproduces"),
    VALUE(HOST_INT, double, 0, "Interaction value from -1 to 1 that hosts should have initially, -2 for random"),
    VALUE(SYM_INT, double, 0, "Interaction value from -1 to 1 that symbionts should have initially, -2 for random"),
    VALUE(GRID_X, int, 5, "Width of the world, just multiplied by the height to get total size"),
    VALUE(GRID_Y, int, 5, "Height of world, just multiplied by width to get total size"),
    VALUE(UPDATES, int, 1, "Number of updates to run before quitting"),
    VALUE(SYM_LIMIT, int, 1, "Number of symbiont allowed to infect a single host"),
    VALUE(LYSIS, bool, 0, "Should lysis occur? 0 for no, 1 for yes"),
    VALUE(HORIZ_TRANS, bool, 0, "Should non-lytic horizontal transmission occur? 0 for no, 1 for yes"),
    VALUE(BURST_SIZE, int, 10, "If there is lysis, this is how many symbionts should be produced during lysis. This will be divided by burst_time and that many symbionts will be produced every update"),
    VALUE(BURST_TIME, int, 10, "If lysis enabled, this is how many updates will pass before lysis occurs"),
    VALUE(HOST_REPRO_RES, double, 1000, "How many resources required for host reproduction"),
    VALUE(SYM_LYSIS_RES, double, 1, "How many resources required for symbiont to create offspring for lysis each update"),
    VALUE(SYM_HORIZ_TRANS_RES, double, 100, "How many resources required for symbiont non-lytic horizontal transmission"),
    VALUE(START_MOI, int, 1, "Ratio of symbionts to hosts that experiment should start with"),
    VALUE(GRID, bool, 0, "Do offspring get placed immediately next to parents on grid, same for symbiont spreading"),
    VALUE(FILE_PATH, string, "", "Output file path"),
    VALUE(FILE_NAME, string, "_data_", "Root output file name")
)

namespace UI = emp::web;

class MyAnimate : public UI::Animate {
private:
  UI::Document doc;

  // Define population
  size_t POP_SIZE = 100;
  size_t GENS = 10000;
  const size_t POP_SIDE = (size_t) std::sqrt(POP_SIZE);
  emp::Random random;
  emp::World<int> grid_world{random};
  emp::vector<emp::Ptr<int>> p;
  int side_x = POP_SIDE;
  int side_y = POP_SIDE;
  const int offset = 20;
  const int RECT_WIDTH = 10;
  int can_size = offset + RECT_WIDTH * POP_SIDE; // set canvas size to be just enough to incorporate petri dish

public: // initialize the class by creating a doc. 

  MyAnimate() : doc("emp_base") {
    // set up our world population params
    grid_world.SetPopStruct_Grid(POP_SIDE, POP_SIDE);

    // How big should each canvas be?
    const double w = can_size;
    const double h = can_size;

    auto mycanvas = doc.AddCanvas(w, h, "can");
    targets.push_back(mycanvas); // targets is an Animation thing that tells you what widget to be refreshed after redraw

    // Add a button that allows for pause and start toggle.
    doc << "<br>";
    doc.AddButton([this](){
        ToggleActive();
        auto but = doc.Button("toggle"); // to access doc, we must capture this, or the current class object, as doc is declared in the scope of this object
        if (GetActive()) but.SetLabel("Pause");
        else but.SetLabel("Start");
      }, "Start", "toggle");

    //doc << UI::Text("fps") << "FPS = " << UI::Live( [this](){return 1000.0 / GetStepTime();} ) ;
    doc << UI::Text("genome") << "Genome = " << UI::Live( [this](){ return grid_world[0]; } );
  }

    void drawPetriDish(UI::Canvas & can){
        int i = 0;
        for (int x = 0; x < side_x; x++){ // now draw a virtual petri dish. 20 is the starting coordinate
            for (int y = 0; y < side_y; y++){
                std::string color;
                if (grid_world[i] < 0) color = "blue";
                else color = "yellow";
                can.Rect(offset + x * RECT_WIDTH, offset + y * RECT_WIDTH, RECT_WIDTH, RECT_WIDTH, color, "black");
                i++;
            }
        }
    }

    void mutate(){
        for (int i = 0; i < grid_world.size(); i++){
            grid_world.InjectAt(random.GetInt(-10, 10), i);
        }
    }

    // a function inherited from the Animation class?
  void DoFrame() {
    auto mycanvas = doc.Canvas("can"); // canvas is added. Here we are getting it by id.
    mycanvas.Clear();

    // Mutate grid_world element and redraw
    mutate();
    drawPetriDish(mycanvas);
    doc.Text("genome").Redraw();
  }
};

MyAnimate anim;


int symbulation_main(int argc, char * argv[]){
    //n_objects(); // some line added to force it to call in order to fix the "js doesn't exist" problem.
    SymConfigBase config;
    //config.Read("SymSettings.cfg"); // comment out to temporarily avoid the file reading issue
    auto args = emp::cl::ArgManager(argc, argv);
    if (args.ProcessConfigOptions(config, std::cout, "SymSettings.cfg") == false) {
      cerr << "There was a problem in processing the options file." << endl;
      exit(1);
    }
    if (args.TestUnknown() == false) {
      cerr << "Leftover args no good." << endl;
      exit(1);
    }
    if (config.BURST_SIZE()%config.BURST_TIME() != 0 && config.BURST_SIZE() < 999999999) {
      cerr << "BURST_SIZE must be an integer multiple of BURST_TIME." << endl;
      exit(1);
    }

    // params
    int numupdates = config.UPDATES();
    int start_moi = config.START_MOI();
    double POP_SIZE = config.GRID_X() * config.GRID_Y();
    bool random_phen_host = true;
    bool random_phen_sym = true;
    if(config.HOST_INT() == -2) random_phen_host = true;
    if(config.SYM_INT() == -2) random_phen_sym = true;

    emp::Random random(config.SEED());
      
    SymWorld world(random);
    if (config.GRID() == 0) world.SetPopStruct_Mixed();
    else world.SetPopStruct_Grid(config.GRID_X(), config.GRID_Y());

    // settings
    world.SetVertTrans(config.VERTICAL_TRANSMISSION());
    world.SetMutRate(config.MUTATION_RATE());
    world.SetSymLimit(config.SYM_LIMIT());
    world.SetLysisBool(config.LYSIS());
    world.SetHTransBool(config.HORIZ_TRANS());
    world.SetBurstSize(config.BURST_SIZE());
    world.SetBurstTime(config.BURST_TIME());
    world.SetHostRepro(config.HOST_REPRO_RES());
    world.SetSymHRes(config.SYM_HORIZ_TRANS_RES());
    world.SetSymLysisRes(config.SYM_LYSIS_RES());
    world.SetSynergy(config.SYNERGY());
    world.SetResPerUpdate(100); 

    int TIMING_REPEAT = config.DATA_INT();
    const bool STAGGER_STARTING_BURST_TIMERS = true;

    //inject organisms
    for (size_t i = 0; i < POP_SIZE; i++){
      Host *new_org;
      if (random_phen_host) new_org = new Host(random.GetDouble(-1, 1));
      else new_org = new Host(config.HOST_INT()); 
          world.Inject(*new_org); 

      for (int j = 0; j < start_moi; j++){ 
        Symbiont new_sym; 
        if(random_phen_sym) new_sym = *(new Symbiont(random.GetDouble(-1, 1)));
        else new_sym = *(new Symbiont(config.SYM_INT()));
        if(STAGGER_STARTING_BURST_TIMERS)
          new_sym.burst_timer = random.GetInt(-5,5);
        world.InjectSymbiont(new_sym); 
      }
    }
    // Random has a seed set. So the random values don't change upon reload of the webpage
    // Draw a virtual petri dish according to population size and color cells by IntVal (interaction value)
    // auto p = world.getPop();
    // int side_x = config.GRID_X();
    // int side_y = config.GRID_Y();
    // int offset = 20; // offset against the left page boundary by 20px

    // Add a Web object for interface manipulation. Settings for web
    // Web web;
    // web.setSideX(side_x);
    // web.setSideY(side_y);
    // web.setOffset(offset);

    // for (size_t i = 0; i < p.size(); i++) doc << p[i]->GetIntVal() << " "; // View initialized values
    // doc << "</br>";

    //auto hostCanvas = web.addHostCanvas(doc); // create a canvas onto doc
    //web.drawPetriDish(hostCanvas, p); // draw a petri dish on the desired canvas
    // Draw petridish has a problem! It's not drawing different columns correctly. 
    return 0;
}

#ifndef CATCH_CONFIG_MAIN
int main(int argc, char * argv[]) {
  return symbulation_main(argc, argv);
}
#endif