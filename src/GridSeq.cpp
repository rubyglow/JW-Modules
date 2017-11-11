#include "JWModules.hpp"
#include "dsp/digital.hpp"

struct GridSeq : Module {
	enum ParamIds {
		RUN_PARAM,
		CLOCK_PARAM,
		RESET_PARAM,
		CELL_NOTE_PARAM,
		CELL_GATE_PARAM = CELL_NOTE_PARAM + 16,
		RND_NOTES_PARAM = CELL_GATE_PARAM + 16,
		ROOT_NOTE_PARAM,
		SCALE_PARAM,
		RND_GATES_PARAM,
		RIGHT_MOVE_BTN_PARAM,
		LEFT_MOVE_BTN_PARAM,
		DOWN_MOVE_BTN_PARAM,
		UP_MOVE_BTN_PARAM,
		RND_MOVE_BTN_PARAM,
		REP_MOVE_BTN_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		EXT_CLOCK_INPUT,
		RESET_INPUT,
		RIGHT_INPUT, LEFT_INPUT, DOWN_INPUT, UP_INPUT,
		REPEAT_INPUT,
		RND_DIR_INPUT,		
		RND_NOTES_INPUT,
		RND_GATES_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		GATES_OUTPUT,
		CELL_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		RUNNING_LIGHT,
		RESET_LIGHT,
		RND_NOTES_LIGHT,
		RND_GATES_LIGHT,
		GATES_LIGHT,
		STEPS_LIGHT = GATES_LIGHT+ 16,
		NUM_LIGHTS = STEPS_LIGHT + 16
	};

	//copied from http://www.grantmuller.com/MidiReference/doc/midiReference/ScaleReference.html
	int SCALE_AEOLIAN        [7] = {0, 2, 3, 5, 7, 8, 10};
	int SCALE_BLUES          [9] = {0, 2, 3, 4, 5, 7, 9, 10, 11};
	int SCALE_CHROMATIC      [12]= {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
	int SCALE_DIATONIC_MINOR [7] = {0, 2, 3, 5, 7, 8, 10};
	int SCALE_DORIAN         [7] = {0, 2, 3, 5, 7, 9, 10};
	int SCALE_HARMONIC_MINOR [7] = {0, 2, 3, 5, 7, 8, 11};
	int SCALE_INDIAN         [7] = {0, 1, 1, 4, 5, 8, 10};
	int SCALE_LOCRIAN        [7] = {0, 1, 3, 5, 6, 8, 10};
	int SCALE_LYDIAN         [7] = {0, 2, 4, 6, 7, 9, 10};
	int SCALE_MAJOR          [7] = {0, 2, 4, 5, 7, 9, 11};
	int SCALE_MELODIC_MINOR  [9] = {0, 2, 3, 5, 7, 8, 9, 10, 11};
	int SCALE_MINOR          [7] = {0, 2, 3, 5, 7, 8, 10};
	int SCALE_MIXOLYDIAN     [7] = {0, 2, 4, 5, 7, 9, 10};
	int SCALE_NATURAL_MINOR  [7] = {0, 2, 3, 5, 7, 8, 10};
	int SCALE_PENTATONIC     [5] = {0, 2, 4, 7, 9};
	int SCALE_PHRYGIAN       [7] = {0, 1, 3, 5, 7, 8, 10};
	int SCALE_TURKISH        [7] = {0, 1, 3, 5, 7, 10, 11};

	SchmittTrigger rightTrigger;
	SchmittTrigger leftTrigger;
	SchmittTrigger downTrigger;
	SchmittTrigger upTrigger;

	SchmittTrigger repeatTrigger;
	SchmittTrigger rndPosTrigger;

	SchmittTrigger runningTrigger;
	SchmittTrigger resetTrigger;
	SchmittTrigger rndNotesTrigger;
	SchmittTrigger rndGatesTrigger;
	SchmittTrigger gateTriggers[16];

	int index = 0;
	int posX = 0;
	int posY = 0;
	float phase = 0.0;
	bool gateState[16] = {};
	bool running = true;
	bool ignoreGateOnPitchOut = false;

	enum GateMode { TRIGGER, RETRIGGER, CONTINUOUS };
	GateMode gateMode = TRIGGER;
	PulseGenerator gatePulse;

	GridSeq() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {}

	void step();

	json_t *toJson() {
		json_t *rootJ = json_object();

		json_object_set_new(rootJ, "running", json_boolean(running));
		json_object_set_new(rootJ, "ignoreGateOnPitchOut", json_boolean(ignoreGateOnPitchOut));

		// gates
		json_t *gatesJ = json_array();
		for (int i = 0; i < 16; i++) {
			json_t *gateJ = json_integer((int) gateState[i]);
			json_array_append_new(gatesJ, gateJ);
		}
		json_object_set_new(rootJ, "gates", gatesJ);

		// gateMode
		json_t *gateModeJ = json_integer((int) gateMode);
		json_object_set_new(rootJ, "gateMode", gateModeJ);

		return rootJ;
	}

	void fromJson(json_t *rootJ) {
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ)
			running = json_is_true(runningJ);

		json_t *ignoreGateOnPitchOutJ = json_object_get(rootJ, "ignoreGateOnPitchOut");
		if (ignoreGateOnPitchOutJ)
			ignoreGateOnPitchOut = json_is_true(ignoreGateOnPitchOutJ);

		// gates
		json_t *gatesJ = json_object_get(rootJ, "gates");
		if (gatesJ) {
			for (int i = 0; i < 16; i++) {
				json_t *gateJ = json_array_get(gatesJ, i);
				if (gateJ)
					gateState[i] = !!json_integer_value(gateJ);
			}
		}

		// gateMode
		json_t *gateModeJ = json_object_get(rootJ, "gateMode");
		if (gateModeJ)
			gateMode = (GateMode)json_integer_value(gateModeJ);
	}

	void reset() {
		for (int i = 0; i < 16; i++) {
			gateState[i] = true;
		}
	}

	void randomize() {
		randomizeGateStates();
	}

	void randomizeGateStates() {
		for (int i = 0; i < 16; i++) {
			gateState[i] = (randomf() > 0.50);
		}
	}

	float getOneRandomNoteInScale(){
		int rootNote = params[ROOT_NOTE_PARAM].value;
		int curScaleVal = params[SCALE_PARAM].value;
		int *curScaleArr;
		int notesInScale = 0;
		switch(curScaleVal){
			case QuantizerWidget::AEOLIAN:        curScaleArr = SCALE_AEOLIAN;       notesInScale=LENGTHOF(SCALE_AEOLIAN); break;
			case QuantizerWidget::BLUES:          curScaleArr = SCALE_BLUES;         notesInScale=LENGTHOF(SCALE_BLUES); break;
			case QuantizerWidget::CHROMATIC:      curScaleArr = SCALE_CHROMATIC;     notesInScale=LENGTHOF(SCALE_CHROMATIC); break;
			case QuantizerWidget::DIATONIC_MINOR: curScaleArr = SCALE_DIATONIC_MINOR;notesInScale=LENGTHOF(SCALE_DIATONIC_MINOR); break;
			case QuantizerWidget::DORIAN:         curScaleArr = SCALE_DORIAN;        notesInScale=LENGTHOF(SCALE_DORIAN); break;
			case QuantizerWidget::HARMONIC_MINOR: curScaleArr = SCALE_HARMONIC_MINOR;notesInScale=LENGTHOF(SCALE_HARMONIC_MINOR); break;
			case QuantizerWidget::INDIAN:         curScaleArr = SCALE_INDIAN;        notesInScale=LENGTHOF(SCALE_INDIAN); break;
			case QuantizerWidget::LOCRIAN:        curScaleArr = SCALE_LOCRIAN;       notesInScale=LENGTHOF(SCALE_LOCRIAN); break;
			case QuantizerWidget::LYDIAN:         curScaleArr = SCALE_LYDIAN;        notesInScale=LENGTHOF(SCALE_LYDIAN); break;
			case QuantizerWidget::MAJOR:          curScaleArr = SCALE_MAJOR;         notesInScale=LENGTHOF(SCALE_MAJOR); break;
			case QuantizerWidget::MELODIC_MINOR:  curScaleArr = SCALE_MELODIC_MINOR; notesInScale=LENGTHOF(SCALE_MELODIC_MINOR); break;
			case QuantizerWidget::MINOR:          curScaleArr = SCALE_MINOR;         notesInScale=LENGTHOF(SCALE_MINOR); break;
			case QuantizerWidget::MIXOLYDIAN:     curScaleArr = SCALE_MIXOLYDIAN;    notesInScale=LENGTHOF(SCALE_MIXOLYDIAN); break;
			case QuantizerWidget::NATURAL_MINOR:  curScaleArr = SCALE_NATURAL_MINOR; notesInScale=LENGTHOF(SCALE_NATURAL_MINOR); break;
			case QuantizerWidget::PENTATONIC:     curScaleArr = SCALE_PENTATONIC;    notesInScale=LENGTHOF(SCALE_PENTATONIC); break;
			case QuantizerWidget::PHRYGIAN:       curScaleArr = SCALE_PHRYGIAN;      notesInScale=LENGTHOF(SCALE_PHRYGIAN); break;
			case QuantizerWidget::TURKISH:        curScaleArr = SCALE_TURKISH;       notesInScale=LENGTHOF(SCALE_TURKISH); break;
		}

		if(curScaleVal == QuantizerWidget::NONE){
			return randomf() * 6.0;
		} else {
			float voltsOut = 0;
			int rndOctaveInVolts = int(5 * randomf());
			voltsOut += rndOctaveInVolts;
			voltsOut += rootNote / 12.0;
			voltsOut += curScaleArr[int(notesInScale * randomf())] / 12.0;
			return voltsOut;
		}
	}

	void randomizeNotesOnly(){
		for (int i = 0; i < 16; i++) {
			params[CELL_NOTE_PARAM + i].value = getOneRandomNoteInScale();
		}
	}

	float closestVoltageInScale(float voltsIn){
		int rootNote = params[ROOT_NOTE_PARAM].value;
		int curScaleVal = params[SCALE_PARAM].value;
		int *curScaleArr;
		int notesInScale = 0;
		switch(curScaleVal){
			case QuantizerWidget::AEOLIAN:        curScaleArr = SCALE_AEOLIAN;       notesInScale=LENGTHOF(SCALE_AEOLIAN); break;
			case QuantizerWidget::BLUES:          curScaleArr = SCALE_BLUES;         notesInScale=LENGTHOF(SCALE_BLUES); break;
			case QuantizerWidget::CHROMATIC:      curScaleArr = SCALE_CHROMATIC;     notesInScale=LENGTHOF(SCALE_CHROMATIC); break;
			case QuantizerWidget::DIATONIC_MINOR: curScaleArr = SCALE_DIATONIC_MINOR;notesInScale=LENGTHOF(SCALE_DIATONIC_MINOR); break;
			case QuantizerWidget::DORIAN:         curScaleArr = SCALE_DORIAN;        notesInScale=LENGTHOF(SCALE_DORIAN); break;
			case QuantizerWidget::HARMONIC_MINOR: curScaleArr = SCALE_HARMONIC_MINOR;notesInScale=LENGTHOF(SCALE_HARMONIC_MINOR); break;
			case QuantizerWidget::INDIAN:         curScaleArr = SCALE_INDIAN;        notesInScale=LENGTHOF(SCALE_INDIAN); break;
			case QuantizerWidget::LOCRIAN:        curScaleArr = SCALE_LOCRIAN;       notesInScale=LENGTHOF(SCALE_LOCRIAN); break;
			case QuantizerWidget::LYDIAN:         curScaleArr = SCALE_LYDIAN;        notesInScale=LENGTHOF(SCALE_LYDIAN); break;
			case QuantizerWidget::MAJOR:          curScaleArr = SCALE_MAJOR;         notesInScale=LENGTHOF(SCALE_MAJOR); break;
			case QuantizerWidget::MELODIC_MINOR:  curScaleArr = SCALE_MELODIC_MINOR; notesInScale=LENGTHOF(SCALE_MELODIC_MINOR); break;
			case QuantizerWidget::MINOR:          curScaleArr = SCALE_MINOR;         notesInScale=LENGTHOF(SCALE_MINOR); break;
			case QuantizerWidget::MIXOLYDIAN:     curScaleArr = SCALE_MIXOLYDIAN;    notesInScale=LENGTHOF(SCALE_MIXOLYDIAN); break;
			case QuantizerWidget::NATURAL_MINOR:  curScaleArr = SCALE_NATURAL_MINOR; notesInScale=LENGTHOF(SCALE_NATURAL_MINOR); break;
			case QuantizerWidget::PENTATONIC:     curScaleArr = SCALE_PENTATONIC;    notesInScale=LENGTHOF(SCALE_PENTATONIC); break;
			case QuantizerWidget::PHRYGIAN:       curScaleArr = SCALE_PHRYGIAN;      notesInScale=LENGTHOF(SCALE_PHRYGIAN); break;
			case QuantizerWidget::TURKISH:        curScaleArr = SCALE_TURKISH;       notesInScale=LENGTHOF(SCALE_TURKISH); break;
			case QuantizerWidget::NONE:           return voltsIn;
		}

		float closestVal = 10.0;
		float closestDist = 10.0;
		int octaveInVolts = int(voltsIn);
		for (int i = 0; i < notesInScale; i++) {
			float scaleNoteInVolts = octaveInVolts + ((rootNote + curScaleArr[i]) / 12.0);
			float distAway = fabs(voltsIn - scaleNoteInVolts);
			if(distAway < closestDist){
				closestVal = scaleNoteInVolts;
				closestDist = distAway;
			}
		}
		return closestVal;
	}

	void handleMoveRight(){ posX = posX == 3 ? 0 : posX + 1; }
	void handleMoveLeft(){ posX = posX == 0 ? 3 : posX - 1; }
	void handleMoveDown(){ posY = posY == 3 ? 0 : posY + 1; }
	void handleMoveUp(){ posY = posY == 0 ? 3 : posY - 1; }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// STEP
///////////////////////////////////////////////////////////////////////////////////////////////////
void GridSeq::step() {
	const float lightLambda = 0.075;
	// Run
	if (runningTrigger.process(params[RUN_PARAM].value)) {
		running = !running;
	}
	lights[RUNNING_LIGHT].value = running ? 1.0 : 0.0;
	
	bool nextStep = false;
	if (resetTrigger.process(params[RESET_PARAM].value + inputs[RESET_INPUT].value)) {
		phase = 0.0;
		posX = 0;
		posY = 0;
		index = 0;
		nextStep = true;
		lights[RESET_LIGHT].value =  1.0;
	}

	if(running){
		if (rndNotesTrigger.process(inputs[RND_NOTES_INPUT].value)) {
			randomizeNotesOnly();
			lights[RND_NOTES_LIGHT].value = 1.0;
		}

		if (rndGatesTrigger.process(inputs[RND_GATES_INPUT].value)) {
			randomizeGateStates();
			lights[RND_GATES_LIGHT].value = 1.0;
		}

		if (repeatTrigger.process(inputs[REPEAT_INPUT].value + params[REP_MOVE_BTN_PARAM].value)) {
			nextStep = true;
		} 
		if (rndPosTrigger.process(inputs[RND_DIR_INPUT].value + params[RND_MOVE_BTN_PARAM].value)) {
			nextStep = true;
			switch(int(4 * randomf())){
				case 0:handleMoveRight();break;
				case 1:handleMoveLeft();break;
				case 2:handleMoveDown();break;
				case 3:handleMoveUp();break;
			}
		} 
		if (rightTrigger.process(inputs[RIGHT_INPUT].value + params[RIGHT_MOVE_BTN_PARAM].value)) {
			nextStep = true;
			handleMoveRight();
		} 
		if (leftTrigger.process(inputs[LEFT_INPUT].value + params[LEFT_MOVE_BTN_PARAM].value)) {
			nextStep = true;
			handleMoveLeft();
		} 
		if (downTrigger.process(inputs[DOWN_INPUT].value + params[DOWN_MOVE_BTN_PARAM].value)) {
			nextStep = true;
			handleMoveDown();
		} 
		if (upTrigger.process(inputs[UP_INPUT].value + params[UP_MOVE_BTN_PARAM].value)) {
			nextStep = true;
			handleMoveUp();
		}
	}
	
	if (nextStep) {
		index = posX + (posY * 4);
		lights[STEPS_LIGHT + index].value = 1.0;
		gatePulse.trigger(1e-3);
	}

	lights[RND_NOTES_LIGHT].value -= lights[RND_NOTES_LIGHT].value / lightLambda / engineGetSampleRate();
	lights[RND_GATES_LIGHT].value -= lights[RND_GATES_LIGHT].value / lightLambda / engineGetSampleRate();
	lights[RESET_LIGHT].value -= lights[RESET_LIGHT].value / lightLambda / engineGetSampleRate();
	bool pulse = gatePulse.process(1.0 / engineGetSampleRate());

	// Gate buttons
	for (int i = 0; i < 16; i++) {
		if (gateTriggers[i].process(params[CELL_GATE_PARAM + i].value)) {
			gateState[i] = !gateState[i];
		}
		bool gateOn = (running && i == index && gateState[i]);
		if (gateMode == TRIGGER)
			gateOn = gateOn && pulse;
		else if (gateMode == RETRIGGER)
			gateOn = gateOn && !pulse;

		if(lights[STEPS_LIGHT + i].value > 0){ lights[STEPS_LIGHT + i].value -= lights[STEPS_LIGHT + i].value / lightLambda / engineGetSampleRate(); }
		lights[GATES_LIGHT + i].value = gateState[i] ? 1.0 - lights[STEPS_LIGHT + i].value : lights[STEPS_LIGHT + i].value;
	}

	// Cells
	bool gatesOn = (running && gateState[index]);
	if (gateMode == TRIGGER)
		gatesOn = gatesOn && pulse;
	else if (gateMode == RETRIGGER)
		gatesOn = gatesOn && !pulse;

	// Outputs
	if(gatesOn || ignoreGateOnPitchOut)	{
		//don't want to change pitch if the step isn't turned on
		outputs[CELL_OUTPUT].value = closestVoltageInScale(params[CELL_NOTE_PARAM + index].value);
	}
	outputs[GATES_OUTPUT].value = gatesOn ? 10.0 : 0.0;
}

struct RandomizeNotesOnlyButton : LEDButton {
	void onMouseDown(EventMouseDown &e) override {
		GridSeqWidget *gsw = this->getAncestorOfType<GridSeqWidget>();
		GridSeq *gs = dynamic_cast<GridSeq*>(gsw->module);
		for (int i = 0; i < 16; i++) {
			if(e.button == 0){
				gsw->seqKnobs[i]->setValue(gs->getOneRandomNoteInScale());
			} else if(e.button == 1){
				//right click this to update the knobs (if randomized by cv in)
				gsw->seqKnobs[i]->setValue(module->params[GridSeq::CELL_NOTE_PARAM + i].value);
			}
		}
	}
};

struct RandomizeGatesOnlyButton : LEDButton {
	void onMouseDown(EventMouseDown &e) override {
		GridSeqWidget *gsw = this->getAncestorOfType<GridSeqWidget>();
		for (int i = 0; i < 16; i++) {
			gsw->gateButtons[i]->setValue(randomf() > 0.5);
		}
	}
};

GridSeqWidget::GridSeqWidget() {
	GridSeq *module = new GridSeq();
	setModule(module);
	box.size = Vec(15*20, 380);

	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(SVG::load(assetPlugin(plugin, "res/GridSeq.svg")));
		addChild(panel);
	}

	addChild(createScrew<Screw_J>(Vec(15, 0)));
	addChild(createScrew<Screw_J>(Vec(15, 365)));
	addChild(createScrew<Screw_W>(Vec(box.size.x-30, 0)));
	addChild(createScrew<Screw_W>(Vec(box.size.x-30, 365)));

	///// RUN AND RESET /////
	addParam(createParam<LEDButton>(Vec(25, 90), module, GridSeq::RUN_PARAM, 0.0, 1.0, 0.0));
	addChild(createLight<SmallLight<MyBlueValueLight>>(Vec(25+5.5, 90+5.5), module, GridSeq::RUNNING_LIGHT));

	addParam(createParam<LEDButton>(Vec(25, 130), module, GridSeq::RESET_PARAM, 0.0, 1.0, 0.0));
	addChild(createLight<SmallLight<MyBlueValueLight>>(Vec(25+5.5, 130+5.5), module, GridSeq::RESET_LIGHT));
	addInput(createInput<PJ301MPort>(Vec(22, 160), module, GridSeq::RESET_INPUT));

	///// DIR CONTROLS /////
	addParam(createParam<RightMoveButton>(Vec(70, 30), module, GridSeq::RIGHT_MOVE_BTN_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<LeftMoveButton>(Vec(103, 30), module, GridSeq::LEFT_MOVE_BTN_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<DownMoveButton>(Vec(137, 30), module, GridSeq::DOWN_MOVE_BTN_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<UpMoveButton>(Vec(172, 30), module, GridSeq::UP_MOVE_BTN_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<RndMoveButton>(Vec(215, 30), module, GridSeq::RND_MOVE_BTN_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<RepMoveButton>(Vec(255, 30), module, GridSeq::REP_MOVE_BTN_PARAM, 0.0, 1.0, 0.0));

	addInput(createInput<PJ301MPort>(Vec(70, 55), module, GridSeq::RIGHT_INPUT));
	addInput(createInput<PJ301MPort>(Vec(103, 55), module, GridSeq::LEFT_INPUT));
	addInput(createInput<PJ301MPort>(Vec(137, 55), module, GridSeq::DOWN_INPUT));
	addInput(createInput<PJ301MPort>(Vec(172, 55), module, GridSeq::UP_INPUT));

	addInput(createInput<PJ301MPort>(Vec(212, 55), module, GridSeq::RND_DIR_INPUT));
	addInput(createInput<PJ301MPort>(Vec(253, 55), module, GridSeq::REPEAT_INPUT));

	///// NOTE AND SCALE CONTROLS /////
	NoteKnob *noteKnob = dynamic_cast<NoteKnob*>(createParam<NoteKnob>(Vec(71, 325), module, GridSeq::ROOT_NOTE_PARAM, 0.0, QuantizerWidget::NUM_NOTES-1, QuantizerWidget::NOTE_C));
	CenteredLabel* const noteLabel = new CenteredLabel;
	noteLabel->box.pos = Vec(41, 180);
	noteLabel->text = "note here";
	noteKnob->connectLabel(noteLabel);
	addChild(noteLabel);
	addParam(noteKnob);

	ScaleKnob *scaleKnob = dynamic_cast<ScaleKnob*>(createParam<ScaleKnob>(Vec(130, 325), module, GridSeq::SCALE_PARAM, 0.0, QuantizerWidget::NUM_SCALES-1, QuantizerWidget::MINOR));
	CenteredLabel* const scaleLabel = new CenteredLabel;
	scaleLabel->box.pos = Vec(71, 180);
	scaleLabel->text = "scale here";
	scaleKnob->connectLabel(scaleLabel);
	addChild(scaleLabel);
	addParam(scaleKnob);

	addParam(createParam<RandomizeNotesOnlyButton>(Vec(235, 330), module, GridSeq::RND_NOTES_PARAM, 0.0, 1.0, 0.0));
	addChild(createLight<SmallLight<MyBlueValueLight>>(Vec(235+5.5, 330+5.5), module, GridSeq::RND_NOTES_LIGHT));
	addInput(createInput<PJ301MPort>(Vec(258, 330-4), module, GridSeq::RND_NOTES_INPUT));

	addParam(createParam<RandomizeGatesOnlyButton>(Vec(178, 330), module, GridSeq::RND_GATES_PARAM, 0.0, 1.0, 0.0));
	addChild(createLight<SmallLight<MyBlueValueLight>>(Vec(178+5.5, 330+5.5), module, GridSeq::RND_GATES_LIGHT));
	addInput(createInput<PJ301MPort>(Vec(200, 330-4), module, GridSeq::RND_GATES_INPUT));

	//// MAIN SEQUENCER KNOBS ////
	int boxSize = 55;
	for (int y = 0; y < 4; y++) {
		for (int x = 0; x < 4; x++) {
			int knobX = x * boxSize + 76;
			int knobY = y * boxSize + 110;
			int idx = (x+(y*4));
			module->gateState[idx] = true; //start with all gates on

			//maybe someday put note labels in each cell
			ParamWidget *cellNoteKnob = createParam<SmallWhiteKnob>(Vec(knobX, knobY), module, GridSeq::CELL_NOTE_PARAM + idx, 0.0, 6.0, 3.0);
			addParam(cellNoteKnob);
			seqKnobs.push_back(cellNoteKnob);

			ParamWidget *cellGateButton = createParam<LEDButton>(Vec(knobX+22, knobY-15), module, GridSeq::CELL_GATE_PARAM + idx, 0.0, 1.0, 0.0);
			addParam(cellGateButton);
			gateButtons.push_back(cellGateButton);

			addChild(createLight<SmallLight<MyBlueValueLight>>(Vec(knobX+27.5, knobY-9.5), module, GridSeq::GATES_LIGHT + idx));
		}
	}

	///// OUTPUTS /////
	addOutput(createOutput<PJ301MPort>(Vec(22, 238), module, GridSeq::GATES_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(22, 300), module, GridSeq::CELL_OUTPUT));
}

struct GridSeqPitchMenuItem : MenuItem {
	GridSeq *gridSeq;
	void onAction(EventAction &e) override {
		gridSeq->ignoreGateOnPitchOut = !gridSeq->ignoreGateOnPitchOut;
	}
	void step() override {
		rightText = (gridSeq->ignoreGateOnPitchOut) ? "✔" : "";
	}
};

struct GridSeqGateModeItem : MenuItem {
	GridSeq *gridSeq;
	GridSeq::GateMode gateMode;
	void onAction(EventAction &e) override {
		gridSeq->gateMode = gateMode;
	}
	void step() override {
		rightText = (gridSeq->gateMode == gateMode) ? "✔" : "";
	}
};

Menu *GridSeqWidget::createContextMenu() {
	Menu *menu = ModuleWidget::createContextMenu();

	MenuLabel *spacerLabel = new MenuLabel();
	menu->pushChild(spacerLabel);

	GridSeq *gridSeq = dynamic_cast<GridSeq*>(module);
	assert(gridSeq);

	MenuLabel *modeLabel = new MenuLabel();
	modeLabel->text = "Gate Mode";
	menu->pushChild(modeLabel);

	GridSeqGateModeItem *triggerItem = new GridSeqGateModeItem();
	triggerItem->text = "Trigger";
	triggerItem->gridSeq = gridSeq;
	triggerItem->gateMode = GridSeq::TRIGGER;
	menu->pushChild(triggerItem);

	GridSeqGateModeItem *retriggerItem = new GridSeqGateModeItem();
	retriggerItem->text = "Retrigger";
	retriggerItem->gridSeq = gridSeq;
	retriggerItem->gateMode = GridSeq::RETRIGGER;
	menu->pushChild(retriggerItem);

	GridSeqGateModeItem *continuousItem = new GridSeqGateModeItem();
	continuousItem->text = "Continuous";
	continuousItem->gridSeq = gridSeq;
	continuousItem->gateMode = GridSeq::CONTINUOUS;
	menu->pushChild(continuousItem);

	MenuLabel *spacerLabel2 = new MenuLabel();
	menu->pushChild(spacerLabel2);

	GridSeqPitchMenuItem *pitchMenuItem = new GridSeqPitchMenuItem();
	pitchMenuItem->text = "Ignore Gate for V/OCT Out";
	pitchMenuItem->gridSeq = gridSeq;
	menu->pushChild(pitchMenuItem);

	return menu;
}

