///////////////////////////////////////////////////
//
//   Master Clock Module
//   VCV Module
//
//   Strum 2017
//
///////////////////////////////////////////////////

#include "mental.hpp"

#include "dsp/digital.hpp"

#include <sstream>
#include <iomanip>

struct LFOGenerator {
	float phase = 0.0;
	float pw = 0.5;
	float freq = 1.0;	
	void setFreq(float freq_to_set)
  {
    freq = freq_to_set;
  }		
	void step(float dt) {
		float deltaPhase = fminf(freq * dt, 0.5);
		phase += deltaPhase;
		if (phase >= 1.0)
			phase -= 1.0;
	}	
	float sqr() {
		float sqr = phase < pw ? 1.0 : -1.0;
		return sqr;
	}
};

struct MentalMasterClock : Module {
	enum ParamIds {    
    TEMPO_PARAM,    
    TIMESIGTOP_PARAM,
    TIMESIGBOTTOM_PARAM,
    RESET_BUTTON,
    RUN_SWITCH,    
		NUM_PARAMS
	};  
	enum InputIds {     
		NUM_INPUTS
	};
	enum OutputIds {
		BEAT_OUT,
    EIGHTHS_OUT,
    SIXTEENTHS_OUT,
    BAR_OUT,       
		NUM_OUTPUTS
	};
  enum LightIds {
		RESET_LED,
    RUN_LED,
		NUM_LIGHTS
	}; 
  
  LFOGenerator clock;
  
  SchmittTrigger eighths_trig;
	SchmittTrigger quarters_trig;
  SchmittTrigger bars_trig;
  
  SchmittTrigger run_button_trig;
  //float run_led = 1.0;
  bool running = true;
  
  int eighths_count = 0;
	int quarters_count = 0;
  int bars_count = 0;
  
  int tempo, time_sig_top, time_sig_bottom = 0;
  float frequency = 2.0;
  int quarters_count_limit = 4;
  int eighths_count_limit = 2;
  int bars_count_limit = 16;
    
  //float reset_led = 0.0;  
  
  MentalMasterClock(); // : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {}
	void step() override;
  
  json_t *toJson() override
  {
		json_t *rootJ = json_object();
    json_t *button_statesJ = json_array();
		json_t *button_stateJ = json_integer((int)running);
		json_array_append_new(button_statesJ, button_stateJ);		
		json_object_set_new(rootJ, "run", button_statesJ);    
    return rootJ;
  }
  
  void fromJson(json_t *rootJ) override
  {
    json_t *button_statesJ = json_object_get(rootJ, "run");
		if (button_statesJ)
    {			
				json_t *button_stateJ = json_array_get(button_statesJ,0);
				if (button_stateJ)
					running = !!json_integer_value(button_stateJ);			
		}  
  }  
};

MentalMasterClock::MentalMasterClock()
{
  params.resize(NUM_PARAMS);
	inputs.resize(NUM_INPUTS);
	outputs.resize(NUM_OUTPUTS);
  lights.resize(NUM_LIGHTS);
  eighths_trig.setThresholds(0.0, 1.0);
  quarters_trig.setThresholds(0.0, 1.0);
  bars_trig.setThresholds(0.0, 1.0);
}

void MentalMasterClock::step()
{
  if (run_button_trig.process(params[RUN_SWITCH].value))
    {
		  running = !running;
	  }
    //run_led = running ? 1.0 : 0.0;
    lights[RUN_LED].value = running ? 1.0 : 0.0;
    
  tempo = std::round(params[TEMPO_PARAM].value);
  time_sig_top = std::round(params[TIMESIGTOP_PARAM].value);
  time_sig_bottom = std::round(params[TIMESIGBOTTOM_PARAM].value);
  time_sig_bottom = std::pow(2,time_sig_bottom+1);
 
  frequency = tempo/60.0;
  if (params[RESET_BUTTON].value > 0.0) 
  {
    eighths_count = 0;
    quarters_count = 0;
    bars_count = 0; 
    lights[RESET_LED].value = 1.0;
  } else lights[RESET_LED].value = 0.0;
  
  if (!running) 
  {
    eighths_count = 0;
    quarters_count = 0;
    bars_count = 0; 
    outputs[BAR_OUT].value = 0.0;
    outputs[BEAT_OUT].value = 0.0;
    outputs[EIGHTHS_OUT].value = 0.0;
    outputs[SIXTEENTHS_OUT].value = 0.0; 
  } else
  {
  if (time_sig_top == time_sig_bottom)
  {
    clock.setFreq(frequency*4);
    quarters_count_limit = 4;
    eighths_count_limit = 2;
    bars_count_limit = 16;    
  } else
  {
    if (time_sig_bottom == 4)
    {
      quarters_count_limit = 4;
      eighths_count_limit = 2;
      bars_count_limit = time_sig_top * 4;  
      clock.setFreq(frequency*4); 
    }
    if (time_sig_bottom == 8)
    {
      quarters_count_limit = 4;
      eighths_count_limit = 2;
      bars_count_limit = time_sig_top * 2;
      clock.setFreq(frequency*4);
      if ((time_sig_top % 3) == 0)
      {
        quarters_count_limit = 6;
        eighths_count_limit = 2;
        bars_count_limit = (time_sig_top/3) * 6;
        clock.setFreq(frequency*6);
      }      
    }
  }
  
  clock.step(1.0 / engineGetSampleRate());
  outputs[SIXTEENTHS_OUT].value = 5.0 * clock.sqr();
 
  if (eighths_trig.process(clock.sqr()) && eighths_count <= eighths_count_limit)
    eighths_count++;
  if (eighths_count >= eighths_count_limit)
  {
    eighths_count = 0;    
  }
  if (eighths_count == 0) outputs[EIGHTHS_OUT].value = 5.0;
  else outputs[EIGHTHS_OUT].value = 0.0;
  
  if (quarters_trig.process(clock.sqr()) && quarters_count <= quarters_count_limit)
    quarters_count++;
  if (quarters_count >= quarters_count_limit)
  {
    quarters_count = 0;    
  }
  if (quarters_count == 0) outputs[BEAT_OUT].value = 5.0;
  else outputs[BEAT_OUT].value = 0.0;
  
  if (bars_trig.process(clock.sqr()) && bars_count <= bars_count_limit)
    bars_count++;
  if (bars_count >= bars_count_limit)
  {
    bars_count = 0;    
  }
  if (bars_count == 0) outputs[BAR_OUT].value = 5.0;
  else outputs[BAR_OUT].value = 0.0; 
  
  }
}

////////////////////////////////////
struct NumberDisplayWidget : TransparentWidget {

  int *value;
  std::shared_ptr<Font> font;

  NumberDisplayWidget() {
    font = Font::load(assetPlugin(plugin, "res/Segment7Standard.ttf"));
  };

  void draw(NVGcontext *vg) override
  {
    // Background
    NVGcolor backgroundColor = nvgRGB(0x00, 0x00, 0x00);
    nvgBeginPath(vg);
    nvgRoundedRect(vg, 0.0, 0.0, box.size.x, box.size.y, 4.0);
    nvgFillColor(vg, backgroundColor);
    nvgFill(vg);    
    
    // text 
    nvgFontSize(vg, 18);
    nvgFontFaceId(vg, font->handle);
    nvgTextLetterSpacing(vg, 2.5);

    std::stringstream to_display;   
    to_display << std::setw(3) << *value;

    Vec textPos = Vec(6.0f, 17.0f);   
    NVGcolor textColor = nvgRGB(0x33, 0x33, 0xff);
    nvgFillColor(vg, textColor);
    nvgText(vg, textPos.x, textPos.y, to_display.str().c_str(), NULL);
  }
};

//////////////////////////////////
MentalMasterClockWidget::MentalMasterClockWidget() {
	MentalMasterClock *module = new MentalMasterClock();
	setModule(module);
	box.size = Vec(15*8, 380);
  
	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		
    panel->setBackground(SVG::load(assetPlugin(plugin,"res/MentalMasterClock.svg")));
		addChild(panel);
	}
   
    addParam(createParam<RoundSmallBlackKnob>(Vec(2, 20), module, MentalMasterClock::TEMPO_PARAM, 40.0, 250.0, 120.0));
    addParam(createParam<RoundSmallBlackKnob>(Vec(2, 50), module, MentalMasterClock::TIMESIGTOP_PARAM,2.0, 15.0, 4.0));
    addParam(createParam<RoundSmallBlackKnob>(Vec(2, 80), module, MentalMasterClock::TIMESIGBOTTOM_PARAM,0.0, 3.0, 1.0));
     
    addOutput(createOutput<PJ301MPort>(Vec(90, 110), module, MentalMasterClock::BEAT_OUT)); 
    addOutput(createOutput<PJ301MPort>(Vec(90, 140), module, MentalMasterClock::BAR_OUT)); 
    addOutput(createOutput<PJ301MPort>(Vec(90, 170), module, MentalMasterClock::EIGHTHS_OUT)); 
    addOutput(createOutput<PJ301MPort>(Vec(90, 200), module, MentalMasterClock::SIXTEENTHS_OUT)); 
    
   /* addParam(createParam<LEDButton>(Vec(5, 50+group_offset*i), module, MentalMasterClock::STEP_SWITCH + i, 0.0, 1.0, 0.0));
    addChild(createLight<MediumLight<GreenLight>>(Vec(10, 55+group_offset*i), &module->button_leds[0][i]));
    addParam(createParam<LEDButton>(Vec(5, 75+group_offset*i), module, MentalMasterClock::BI_SWITCH + i, 0.0, 1.0, 0.0));
    addChild(createLight<MediumLight<GreenLight>>(Vec(10, 80+group_offset*i), &module->button_leds[2][i])); */
    
    //addChild(createLight<MediumLight<GreenRedLight>>(Vec(33, 125), module, MentalClockDivider::LIGHTS));
    
    addParam(createParam<LEDButton>(Vec(5, 140), module, MentalMasterClock::RESET_BUTTON, 0.0, 1.0, 0.0));
    addChild(createLight<MediumLight<GreenLight>>(Vec(10, 145), module, MentalMasterClock::RESET_LED));
    
    addParam(createParam<LEDButton>(Vec(5, 110), module, MentalMasterClock::RUN_SWITCH, 0.0, 1.0, 0.0));
    addChild(createLight<MediumLight<GreenLight>>(Vec(10, 115), module, MentalMasterClock::RUN_LED));
    
  NumberDisplayWidget *display = new NumberDisplayWidget();
	display->box.pos = Vec(35,20);
	display->box.size = Vec(50, 20);
	display->value = &module->tempo;
	addChild(display); 
    
  NumberDisplayWidget *display2 = new NumberDisplayWidget();
	display2->box.pos = Vec(35,50);
	display2->box.size = Vec(50, 20);
	display2->value = &module->time_sig_top;
	addChild(display2); 
  
  NumberDisplayWidget *display3 = new NumberDisplayWidget();
	display3->box.pos = Vec(35,80);
	display3->box.size = Vec(50, 20);
	display3->value = &module->time_sig_bottom;
	addChild(display3); 
 
}
