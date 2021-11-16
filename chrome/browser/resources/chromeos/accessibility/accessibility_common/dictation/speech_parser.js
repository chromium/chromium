// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {InputController} from './input_controller.js';
import {InputTextViewMacro, NewLineMacro} from './macros/input_text_view_macro.js';
import {ListCommandsMacro} from './macros/list_commands_macro.js';
import {Macro} from './macros/macro.js';
import {MacroName} from './macros/macro_names.js';
import * as RepeatableKeyPressMacro from './macros/repeatable_key_press_macro.js';
// PumpkinAvailability is based on the gn argument enable_pumpkin_for_dictation,
// and pumpkin_availability.js is copied from either include_pumpkin.js
// or exclude_pumpkin.js in the BUILD rule.
import {PumpkinAvailability} from './pumpkin/pumpkin_availability.js';


// TODO(crbug.com/1264544): Consider refactoring into a class.
/**
 * @typedef {{
 *   commandString: string,
 *   createMacro: function():Macro,
 *   matchesMacro: function(string):boolean,
 *   matchesInputTextViewMacro: function(string):boolean,
 * }}
 */
let ActionInfo;

/**
 * PumpkinTagger Hypothesis argument names. These should match the variable
 * argument placeholders in voiceaccess.patterns_template and the static strings
 * defined in voiceaccess/utils/PumpkinUtils.java in google3.
 * @enum {string}
 */
const HypothesisArgumentName = {
  SEM_TAG: 'SEM_TAG',
  NUM_ARG: 'NUM_ARG',
  OPEN_ENDED_TEXT: 'OPEN_ENDED_TEXT',
};

/**
 * SpeechParser handles parsing spoken transcripts into Macros.
 */
export class SpeechParser {
  /**
   * @param {InputController} inputController to interact with the IME.
   */
  constructor(inputController) {
    /** @private {boolean} */
    this.commandsFeatureEnabled_ = false;

    /**
     * Map of command action IDs to information about that command.
     * This object uses localized strings.
     * @private {!Map<MacroName, ActionInfo>}
     */
    this.commandMap_ = new Map();

    /** @private {boolean} */
    this.isRTLLocale_ = false;

    /** @private {speech.pumpkin.api.js.PumpkinTagger.PumpkinTagger} */
    this.pumpkinTagger_ = null;

    /** @private {InputController} */
    this.inputController_ = inputController;

    /** @private {?Promise} */
    this.pumpkinLoadingPromise_ = null;
  }

  /**
   * Parses user text to produce a macro command. Async to allow pumpkin to
   * complete loading if needed.
   * @param {string} text The text to parse.
   * @return {Promise<Macro>}
   */
  async parse(text) {
    if (!this.commandsFeatureEnabled_) {
      // Without ExperimentalAccessibilityDictationCommands feature, all
      // text should be input as-is.
      return new Promise(resolve => {
        resolve(new InputTextViewMacro(text, this.inputController_));
      });
    }

    // Pumpkin load requires several async calls. If the request to parse
    // comes before load is complete, wait for load. This happens during
    // browser tests which may be fast enough to start sending speech text
    // before callbacks with user prefs have completed.
    if (this.pumpkinLoadingPromise_) {
      await this.pumpkinLoadingPromise_;
    }

    return new Promise(resolve => {
      resolve(this.parseWithCommandsEnabled_(text));
    });
  }

  /**
   * Private method to parse user text to produce a macro when commands are
   * enabled.
   * @param {string} text
   * @return {Macro}
   * @private
   */
  parseWithCommandsEnabled_(text) {
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

    // If Pumpkin is not available we can fall back to a hard-coded list
    // of possible commands. First, see if the text matches a command action.
    for (const [action, info] of this.commandMap_) {
      if (info.matchesMacro(text)) {
        return info.createMacro();
      } else if (info.matchesInputTextViewMacro(text)) {
        text = info.commandString;
        break;
      }
    }

    // The command is simply to input the given text.
    return new InputTextViewMacro(text, this.inputController_);
  }

  /**
   * Enables commands, either with Pumpkin semantic parsing or Regular
   * Expressions.
   * @param {string} locale The Dictation recognition locale. Only some locales
   *     are supported by Pumpkin.
   */
  setCommandsEnabled(locale) {
    this.isRTLLocale_ = SpeechParser.RTLLocales.has(locale);
    this.commandsFeatureEnabled_ = true;
    if (PumpkinAvailability.usePumpkin(locale)) {
      // Loads Pumpkin web assembly.
      this.initPumpkin_(PumpkinAvailability.LOCALES[locale]);
    } else if (this.pumpkinTagger_) {
      // Pumpkin isn't available for this locale even though it was previously
      // available.
      this.pumpkinTagger_ = null;
    }

    // Does pre-work to parse commands: gets translated command strings and
    // generates a map of commands to regular expressions that would match them.
    // These are used as a fall-back when Pumpkin is not available.
    // TODO(crbug.com/1264544): Don't generate commands for all names when
    // Pumpkin is available. Just generate "new line" (the only non-pumpkin
    // command) or nothing at all.
    for (const key in MacroName) {
      const actionId = MacroName[key];
      let messageId;
      let createMacro;
      switch (actionId) {
        case MacroName.DELETE_PREV_CHAR:
          messageId = 'dictation_command_delete_prev_char';
          createMacro = () => {
            return new RepeatableKeyPressMacro.DeletePreviousCharacterMacro();
          };
          break;
        case MacroName.NAV_PREV_CHAR:
          messageId = 'dictation_command_nav_prev_char';
          createMacro = () => {
            return new RepeatableKeyPressMacro.NavPreviousCharMacro(
                this.isRTLLocale_);
          };
          break;
        case MacroName.NAV_NEXT_CHAR:
          messageId = 'dictation_command_nav_next_char';
          createMacro = () => {
            return new RepeatableKeyPressMacro.NavNextCharMacro(
                this.isRTLLocale_);
          };
          break;
        case MacroName.NAV_PREV_LINE:
          messageId = 'dictation_command_nav_prev_line';
          createMacro = () => {
            return new RepeatableKeyPressMacro.NavPreviousLineMacro();
          };
          break;
        case MacroName.NAV_NEXT_LINE:
          messageId = 'dictation_command_nav_next_line';
          createMacro = () => {
            return new RepeatableKeyPressMacro.NavNextLineMacro();
          };
          break;
        case MacroName.COPY_SELECTED_TEXT:
          messageId = 'dictation_command_copy_selected_text';
          createMacro = () => {
            return new RepeatableKeyPressMacro.CopySelectedTextMacro();
          };
          break;
        case MacroName.PASTE_TEXT:
          messageId = 'dictation_command_paste_text';
          createMacro = () => {
            return new RepeatableKeyPressMacro.PasteTextMacro();
          };
          break;
        case MacroName.CUT_SELECTED_TEXT:
          messageId = 'dictation_command_cut_selected_text';
          createMacro = () => {
            return new RepeatableKeyPressMacro.CutSelectedTextMacro();
          };
          break;
        case MacroName.UNDO_TEXT_EDIT:
          messageId = 'dictation_command_undo_text_edit';
          createMacro = () => {
            return new RepeatableKeyPressMacro.UndoTextEditMacro();
          };
          break;
        case MacroName.REDO_ACTION:
          messageId = 'dictation_command_redo_action';
          createMacro = () => {
            return new RepeatableKeyPressMacro.RedoActionMacro();
          };
          break;
        case MacroName.SELECT_ALL_TEXT:
          messageId = 'dictation_command_select_all_text';
          createMacro = () => {
            return new RepeatableKeyPressMacro.SelectAllTextMacro();
          };
          break;
        case MacroName.UNSELECT_TEXT:
          messageId = 'dictation_command_unselect_text';
          createMacro = () => {
            return new RepeatableKeyPressMacro.UnselectTextMacro(
                this.isRTLLocale_);
          };
          break;
        case MacroName.LIST_COMMANDS:
          messageId = 'dictation_command_list_commands';
          createMacro = () => {
            return new ListCommandsMacro();
          };
          break;
        case MacroName.NEW_LINE:
          messageId = 'dictation_command_new_line';
          createMacro = () => {
            return new NewLineMacro(this.inputController_);
          };
          break;
        default:
          // Other macros are not supported by regular expressions and
          // should go through Pumpkin. Strongly encouraged not to add any
          // new actions manually unless they are needed by users who do not
          // use languages with Pumpkin translations.
          continue;
      }
      const commandString = chrome.i18n.getMessage(messageId);
      this.commandMap_.set(actionId, {
        commandString,
        createMacro,
        matchesMacro: this.commandMatcher_(commandString),
        matchesInputTextViewMacro: this.inputTextViewMatcher_(commandString)
      });
    }
  }

  /**
   * Initializes Pumpkin by loading the required scripts and creating the
   * PumpkinTagger object.
   * @param {string} locale The locale in which to init Pumpkin actions.
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
   * @return {Promise<boolean>} Whether the tagger was created successfully.
   * @private
   */
  async createPumpkinTagger_(locale) {
    const pumpkinTagger =
        new speech.pumpkin.api.js.PumpkinTagger.PumpkinTagger();
    try {
      const path = `dictation/pumpkin/${locale}/`;
      let success = await pumpkinTagger.initializeFromPumpkinConfig(
          `${path}pumpkin_config.binarypb`);
      if (!success) {
        console.warn('Failed to load PumpkinTagger from PumpkinConfig.');
        return false;
      }
      success =
          await pumpkinTagger.loadActionFrame(`${path}action_config.binarypb`);
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
   * @private
   */
  async loadPumpkinScripts_() {
    const pumpkinTaggerScript =
        /** @type {!HTMLScriptElement} */ (document.createElement('script'));
    pumpkinTaggerScript.src = 'dictation/pumpkin/js_pumpkin_tagger_bin.js';
    const taggerLoadPromise = new Promise((resolve, reject) => {
      pumpkinTaggerScript.addEventListener('load', () => {
        resolve();
      });
    });
    document.head.appendChild(pumpkinTaggerScript);
    await taggerLoadPromise;

    const wasmModuleScript =
        /** @type {!HTMLScriptElement} */ (document.createElement('script'));
    wasmModuleScript.src = 'dictation/pumpkin/tagger_wasm_main.js';
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
      if (argument.name === HypothesisArgumentName.SEM_TAG) {
        tag = MacroName[argument.value];
      } else if (argument.name === HypothesisArgumentName.NUM_ARG) {
        repeat = argument.value;
      } else if (argument.name === HypothesisArgumentName.OPEN_ENDED_TEXT) {
        text = argument.value;
      }
    }
    switch (tag) {
      case MacroName.INPUT_TEXT_VIEW:
        return new InputTextViewMacro(text, this.inputController_);
      case MacroName.DELETE_PREV_CHAR:
        return new RepeatableKeyPressMacro.DeletePreviousCharacterMacro(repeat);
      case MacroName.NAV_PREV_CHAR:
        return new RepeatableKeyPressMacro.NavPreviousCharMacro(
            this.isRTLLocale_, repeat);
      case MacroName.NAV_NEXT_CHAR:
        return new RepeatableKeyPressMacro.NavNextCharMacro(
            this.isRTLLocale_, repeat);
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
        return new RepeatableKeyPressMacro.UnselectTextMacro(this.isRTLLocale_);
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

  /**
   * Gets a function that checks whether a string matches a
   * command string, ignoring case and whitespace.
   * @param {string} commandString
   * @return {function(string):boolean}
   * @private
   */
  commandMatcher_(commandString) {
    const re = new RegExp('^[\\s]*' + commandString + '[\\s]*$', 'im');
    return text => {
      return re.exec(text);
    };
  }

  /**
   * Gets a function that checks whether a string matches
   * a request to type a command, i.e. for the command 'delete', it would
   * match 'type delete', ignoring case and whitespace.
   * @param {string} commandString
   * @return {function(string):boolean}
   * @private
   */
  inputTextViewMatcher_(commandString) {
    const query = chrome.i18n.getMessage(
        'dictation_command_input_text_view', commandString);
    const re = new RegExp('^[\\s]*' + query + '[\\s]*$', 'im');
    return text => {
      return re.exec(text);
    };
  }
}

// All RTL locales from Dictation::GetAllSupportedLocales.
SpeechParser.RTLLocales = new Set([
  'ar-AE', 'ar-BH', 'ar-DZ', 'ar-EG', 'ar-IL', 'ar-IQ', 'ar-JO',
  'ar-KW', 'ar-LB', 'ar-MA', 'ar-OM', 'ar-PS', 'ar-QA', 'ar-SA',
  'ar-TN', 'ar-YE', 'fa-IR', 'iw-IL', 'ur-IN', 'ur-PK'
]);
