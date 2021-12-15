// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines a strategy for parsing text that utilizes the pumpkin
 * semantic parser.
 */

import {InputController} from './../input_controller.js';
import {InputTextViewMacro} from './../macros/input_text_view_macro.js';
import {ListCommandsMacro} from './../macros/list_commands_macro.js';
import {Macro} from './../macros/macro.js';
import {MacroName} from './../macros/macro_names.js';
import * as RepeatableKeyPressMacro from './../macros/repeatable_key_press_macro.js';
import {ParseStrategy} from './parse_strategy.js';
// PumpkinAvailability is based on the gn argument enable_pumpkin_for_dictation,
// and pumpkin_availability.js is copied from either include_pumpkin.js
// or exclude_pumpkin.js in the BUILD rule.
import {PumpkinAvailability} from './pumpkin/pumpkin_availability.js';

/** A parsing strategy that utilizes the Pumpkin semantic parser. */
export class PumpkinParseStrategy extends ParseStrategy {
  /**
   * @param {!InputController} inputController
   * @param {boolean} isRTLLocale
   * @param {string} locale
   * @return {!Promise<!PumpkinParseStrategy>}
   */
  static async create(inputController, isRTLLocale, locale) {
    const instance = new PumpkinParseStrategy(inputController, isRTLLocale);
    if (PumpkinAvailability.usePumpkin(locale)) {
      await instance.initPumpkin_(PumpkinAvailability.LOCALES[locale]);
    }

    return instance;
  }

  /**
   * @param {!InputController} inputController
   * @param {boolean} isRTLLocale
   * @private
   */
  constructor(inputController, isRTLLocale) {
    super(inputController, isRTLLocale);

    /** @private {speech.pumpkin.api.js.PumpkinTagger.PumpkinTagger} */
    this.pumpkinTagger_ = null;

    /** @private {?Promise} */
    this.pumpkinLoadingPromise_ = null;
  }

  /**
   * Initializes Pumpkin by loading the required scripts and creating the
   * PumpkinTagger object.
   * @param {string} locale The locale in which to init Pumpkin actions.
   * @return {!Promise<undefined>}
   * @private
   */
  async initPumpkin_(locale) {
    if (this.pumpkinLoadingPromise_) {
      // Already initializing.
      return;
    }

    this.pumpkinLoadingPromise_ =
        new Promise(async (pumpkinLoadResolve, pumpkinLoadReject) => {
          // Check for objects defined by the Pumpkin WASM.
          if (!goog || !goog['global'] || !goog['global']['Module']) {
            await this.loadPumpkinScripts_();
          }
          const success = await this.createPumpkinTagger_(locale);
          if (success) {
            pumpkinLoadResolve();
          } else {
            pumpkinLoadReject();
          }
        });
  }

  /**
   * Creates a PumpkinTagger from a config and action frame file for a
   * particular locale.
   * @param {string} locale The locale in which to init Pumpkin actions.
   * @return {!Promise<boolean>} Whether the tagger was created successfully.
   * @private
   */
  async createPumpkinTagger_(locale) {
    const pumpkinTagger =
        new speech.pumpkin.api.js.PumpkinTagger.PumpkinTagger();
    try {
      const path = `${PumpkinParseStrategy.PUMPKIN_DIR}${locale}/`;
      let success = await pumpkinTagger.initializeFromPumpkinConfig(
          `${path}${PumpkinParseStrategy.PUMPKIN_CONFIG_PROTO_SRC}`);
      if (!success) {
        console.warn('Failed to load PumpkinTagger from PumpkinConfig.');
        return false;
      }
      success = await pumpkinTagger.loadActionFrame(
          `${path}${PumpkinParseStrategy.PUMPKIN_ACTION_CONFIG_PROTO_SRC}`);
      if (!success) {
        console.warn('Failed to load Pumpkin ActionConfig.');
        return false;
      }
    } catch (e) {
      console.warn('Error initializing PumpkinTagger', e);
      return false;
    }
    this.pumpkinTagger_ = pumpkinTagger;
    return true;
  }

  /**
   * Loads the Pumpkin scripts javascript in to the document.
   * @return {!Promise<undefined>}
   * @private
   */
  async loadPumpkinScripts_() {
    const pumpkinTaggerScript =
        /** @type {!HTMLScriptElement} */ (document.createElement('script'));
    pumpkinTaggerScript.src = PumpkinParseStrategy.PUMPKIN_TAGGER_SRC;
    const taggerLoadPromise = new Promise((resolve, reject) => {
      pumpkinTaggerScript.addEventListener('load', () => {
        resolve();
      });
    });
    document.head.appendChild(pumpkinTaggerScript);
    await taggerLoadPromise;

    const wasmModuleScript =
        /** @type {!HTMLScriptElement} */ (document.createElement('script'));
    wasmModuleScript.src = PumpkinParseStrategy.PUMPKIN_WASM_SRC;
    const moduleLoadPromise = new Promise((resolve, reject) => {
      goog['global']['Module'] = {
        onRuntimeInitialized() {
          resolve();
        }
      };
    });
    document.head.appendChild(wasmModuleScript);
    await moduleLoadPromise;
  }

  /**
   * In Android Voice Access, Pumpkin Hypotheses will be converted to UserIntent
   * protos before being passed to Macros.
   * @param {proto.speech.pumpkin.HypothesisResult.ObjectFormat} hypothesis
   * @return {?Macro} The macro matching the hypothesis if one can be found.
   * @private
   */
  macroFromPumpkinHypothesis_(hypothesis) {
    const numArgs = hypothesis.actionArgumentList.length;
    if (!numArgs) {
      return null;
    }
    let repeat = 1;
    let text = '';
    let tag = '';
    for (let i = 0; i < numArgs; i++) {
      const argument = hypothesis.actionArgumentList[i];
      // See Variable Argument Placeholders in voiceaccess.patterns_template.
      if (argument.name ===
          PumpkinParseStrategy.HypothesisArgumentName.SEM_TAG) {
        tag = MacroName[argument.value];
      } else if (
          argument.name ===
          PumpkinParseStrategy.HypothesisArgumentName.NUM_ARG) {
        repeat = argument.value;
      } else if (
          argument.name ===
          PumpkinParseStrategy.HypothesisArgumentName.OPEN_ENDED_TEXT) {
        text = argument.value;
      }
    }
    switch (tag) {
      case MacroName.INPUT_TEXT_VIEW:
        return new InputTextViewMacro(text, this.getInputController());
      case MacroName.DELETE_PREV_CHAR:
        return new RepeatableKeyPressMacro.DeletePreviousCharacterMacro(repeat);
      case MacroName.NAV_PREV_CHAR:
        return new RepeatableKeyPressMacro.NavPreviousCharMacro(
            this.getIsRTLLocale(), repeat);
      case MacroName.NAV_NEXT_CHAR:
        return new RepeatableKeyPressMacro.NavNextCharMacro(
            this.getIsRTLLocale(), repeat);
      case MacroName.NAV_PREV_LINE:
        return new RepeatableKeyPressMacro.NavPreviousLineMacro(repeat);
      case MacroName.NAV_NEXT_LINE:
        return new RepeatableKeyPressMacro.NavNextLineMacro(repeat);
      case MacroName.COPY_SELECTED_TEXT:
        return new RepeatableKeyPressMacro.CopySelectedTextMacro();
      case MacroName.PASTE_TEXT:
        return new RepeatableKeyPressMacro.PasteTextMacro();
      case MacroName.CUT_SELECTED_TEXT:
        return new RepeatableKeyPressMacro.CutSelectedTextMacro();
      case MacroName.UNDO_TEXT_EDIT:
        return new RepeatableKeyPressMacro.UndoTextEditMacro();
      case MacroName.REDO_ACTION:
        return new RepeatableKeyPressMacro.RedoActionMacro();
      case MacroName.SELECT_ALL_TEXT:
        return new RepeatableKeyPressMacro.SelectAllTextMacro();
      case MacroName.UNSELECT_TEXT:
        return new RepeatableKeyPressMacro.UnselectTextMacro(
            this.getIsRTLLocale());
      case MacroName.LIST_COMMANDS:
        return new ListCommandsMacro();
      default:
        // Every hypothesis is guaranteed to include a semantic tag due to the
        // way Voice Access set up its grammars. Not all tags are supported in
        // Dictation yet.
        console.log('Unsupported Pumpkin action: ', tag);
        return null;
    }
  }

  /** @override */
  async parse(text) {
    // Pumpkin load requires several async calls. If the request to parse
    // comes before load is complete, wait for load. This happens during
    // browser tests which may be fast enough to start sending speech text
    // before callbacks with user prefs have completed.
    if (this.pumpkinLoadingPromise_) {
      await this.pumpkinLoadingPromise_;
    }

    // Try to get results from Pumpkin.
    // TODO(crbug.com/1264544): Could increase the hypotheses count from 1
    // when we are ready to implement disambiguation.
    if (this.pumpkinTagger_) {
      // Try to get results from Pumpkin.
      // TODO(crbug.com/1264544): Could increase the hypotheses count from 1
      // when we are ready to implement disambiguation.
      const taggerResults =
          this.pumpkinTagger_.tagAndGetNBestHypotheses(text, 1);
      if (taggerResults && taggerResults.hypothesisList.length > 0) {
        const macro =
            this.macroFromPumpkinHypothesis_(taggerResults.hypothesisList[0]);
        if (macro) {
          return macro;
        }
      }
    }

    return null;
  }
}

/**
 * PumpkinTagger Hypothesis argument names. These should match the variable
 * argument placeholders in voiceaccess.patterns_template and the static strings
 * defined in voiceaccess/utils/PumpkinUtils.java in google3.
 * @enum {string}
 */
PumpkinParseStrategy.HypothesisArgumentName = {
  SEM_TAG: 'SEM_TAG',
  NUM_ARG: 'NUM_ARG',
  OPEN_ENDED_TEXT: 'OPEN_ENDED_TEXT',
};

/**
 * The pumpkin/ directory, relative to the accessibility common base directory.
 * @type {string}
 * @const
 */
PumpkinParseStrategy.PUMPKIN_DIR = 'dictation/parse/pumpkin/';

/**
 * The path to the pumpkin tagger source file.
 * @type {string}
 * @const
 */
PumpkinParseStrategy.PUMPKIN_TAGGER_SRC =
    PumpkinParseStrategy.PUMPKIN_DIR + 'js_pumpkin_tagger_bin.js';

/**
 * The path to the pumpkin web assembly module source file.
 * @type {string}
 * @const
 */
PumpkinParseStrategy.PUMPKIN_WASM_SRC =
    PumpkinParseStrategy.PUMPKIN_DIR + 'tagger_wasm_main.js';

/**
 * The name of the pumpkin config binary proto file.
 * @type {string}
 * @const
 */
PumpkinParseStrategy.PUMPKIN_CONFIG_PROTO_SRC = 'pumpkin_config.binarypb';

/**
 * The name of the pumpkin action config binary proto file.
 * @type {string}
 * @const
 */
PumpkinParseStrategy.PUMPKIN_ACTION_CONFIG_PROTO_SRC = 'action_config.binarypb';
