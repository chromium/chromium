// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines a simple strategy for parsing text and converting
 * it into a Macro.
 */

import {InputController} from '/common/action_fulfillment/input_controller.js';
import {DeletePrevSentMacro} from '/common/action_fulfillment/macros/delete_prev_sent_macro.js';
import {InputTextViewMacro, NewLineMacro} from '/common/action_fulfillment/macros/input_text_view_macro.js';
import {Macro} from '/common/action_fulfillment/macros/macro.js';
import {MacroName} from '/common/action_fulfillment/macros/macro_names.js';
import {NavNextSentMacro, NavPrevSentMacro} from '/common/action_fulfillment/macros/nav_sent_macro.js';
import {RepeatMacro} from '/common/action_fulfillment/macros/repeat_macro.js';
import * as RepeatableKeyPress from '/common/action_fulfillment/macros/repeatable_key_press_macro.js';
import {SmartDeletePhraseMacro} from '/common/action_fulfillment/macros/smart_delete_phrase_macro.js';
import {SmartInsertBeforeMacro} from '/common/action_fulfillment/macros/smart_insert_before_macro.js';
import {SmartReplacePhraseMacro} from '/common/action_fulfillment/macros/smart_replace_phrase_macro.js';
import {SmartSelectBetweenMacro} from '/common/action_fulfillment/macros/smart_select_between_macro.js';
import {ToggleDictationMacro} from '/common/action_fulfillment/macros/toggle_dictation_macro.js';

import {LocaleInfo} from '../locale_info.js';
import {ListCommandsMacro} from '../macros/list_commands_macro.js';

import {ParseStrategy} from './parse_strategy.js';

/**
 * @typedef {{
 *   messageId: string,
 *   build: !Function,
 * }}
 */
let MacroData;

/**
 * SimpleMacroFactory converts spoken strings into Macros using string matching.
 */
class SimpleMacroFactory {
  /**
   * @param {!MacroName} macroName
   * @param {!InputController} inputController
   */
  constructor(macroName, inputController) {
    if (!SimpleMacroFactory.getData_()[macroName]) {
      throw new Error(
          'Macro is not supported by SimpleMacroFactory: ' + macroName);
    }

    /** @private {!MacroName} */
    this.macroName_ = macroName;
    /** @private {!InputController} */
    this.inputController_ = inputController;

    /** @private {RegExp} */
    this.commandRegex_ = null;
    this.initializeCommandRegex_(this.macroName_);
  }

  /**
   * Builds a RegExp that can be used to parse a command. For example, the
   * SmartReplacePhraseMacro can be parsed with the pattern:
   * /replace (.*) with (.*)/i.
   * @param {!MacroName} macroName
   * @private
   */
  initializeCommandRegex_(macroName) {
    const matchAnythingPattern = '(.*)';
    const args = [];
    switch (macroName) {
      case MacroName.INPUT_TEXT_VIEW:
      case MacroName.SMART_DELETE_PHRASE:
        args.push(matchAnythingPattern);
        break;
      case MacroName.SMART_REPLACE_PHRASE:
      case MacroName.SMART_INSERT_BEFORE:
      case MacroName.SMART_SELECT_BTWN_INCL:
        args.push(matchAnythingPattern, matchAnythingPattern);
        break;
    }
    const message = chrome.i18n.getMessage(
        SimpleMacroFactory.getData_()[macroName].messageId, args);
    const pattern = `^${message}$`;
    if (LocaleInfo.considerSpaces()) {
      this.commandRegex_ = new RegExp(pattern, 'i');
    } else {
      // A regex to be used if the Dictation language doesn't use spaces e.g.
      // Japanese.
      this.commandRegex_ = new RegExp(pattern.replace(/\s+/g, ''), 'i');
    }
  }

  /**
   * @param {string} text
   * @return {Macro|null}
   */
  createMacro(text) {
    // Check whether `text` matches `this.commandRegex_`, ignoring case and
    // whitespace.
    text = text.trim().toLowerCase();
    if (!this.commandRegex_.test(text)) {
      return null;
    }

    const initialArgs = [];
    switch (this.macroName_) {
      case MacroName.COPY_SELECTED_TEXT:
      case MacroName.CUT_SELECTED_TEXT:
      case MacroName.DELETE_ALL_TEXT:
      case MacroName.DELETE_PREV_CHAR:
      case MacroName.DELETE_PREV_SENT:
      case MacroName.DELETE_PREV_WORD:
      case MacroName.NAV_END_TEXT:
      case MacroName.NAV_NEXT_CHAR:
      case MacroName.NAV_NEXT_LINE:
      case MacroName.NAV_NEXT_SENT:
      case MacroName.NAV_NEXT_WORD:
      case MacroName.NAV_START_TEXT:
      case MacroName.NAV_PREV_CHAR:
      case MacroName.NAV_PREV_LINE:
      case MacroName.NAV_PREV_SENT:
      case MacroName.NAV_PREV_WORD:
      case MacroName.NEW_LINE:
      case MacroName.SELECT_ALL_TEXT:
      case MacroName.SELECT_NEXT_CHAR:
      case MacroName.SELECT_NEXT_WORD:
      case MacroName.SELECT_PREV_CHAR:
      case MacroName.SELECT_PREV_WORD:
      case MacroName.SMART_DELETE_PHRASE:
      case MacroName.SMART_INSERT_BEFORE:
      case MacroName.SMART_REPLACE_PHRASE:
      case MacroName.SMART_SELECT_BTWN_INCL:
      case MacroName.UNSELECT_TEXT:
        initialArgs.push(this.inputController_);
        break;
    }

    const result = this.commandRegex_.exec(text);
    // `result[0]` contains the entire matched text, while all subsequent
    // indices contain text matched by each /(.*)/. We're only interested in
    // text matched by /(.*)/, so ignore `result[0]`.
    const extractedArgs = result.slice(1);
    const finalArgs = initialArgs.concat(extractedArgs);
    const data = SimpleMacroFactory.getData_();
    const macro = new data[this.macroName_].build(...finalArgs);
    if (macro.isSmart() && !LocaleInfo.allowSmartEditing()) {
      return null;
    }

    return macro;
  }

  /**
   * Returns data that is used to create a macro. `messageId` is used to
   * retrieve the macro's command string and `build` is used to construct the
   * macro.
   * @return {Object<MacroName, MacroData>}
   * @private
   */
  static getData_() {
    return {
      [MacroName.DELETE_PREV_CHAR]: {
        messageId: 'dictation_command_delete_prev_char',
        build: RepeatableKeyPress.DeletePreviousCharacterMacro,
      },
      [MacroName.NAV_PREV_CHAR]: {
        messageId: 'dictation_command_nav_prev_char',
        build: RepeatableKeyPress.NavPreviousCharMacro,
      },
      [MacroName.NAV_NEXT_CHAR]: {
        messageId: 'dictation_command_nav_next_char',
        build: RepeatableKeyPress.NavNextCharMacro,
      },
      [MacroName.NAV_PREV_LINE]: {
        messageId: 'dictation_command_nav_prev_line',
        build: RepeatableKeyPress.NavPreviousLineMacro,
      },
      [MacroName.NAV_NEXT_LINE]: {
        messageId: 'dictation_command_nav_next_line',
        build: RepeatableKeyPress.NavNextLineMacro,
      },
      [MacroName.COPY_SELECTED_TEXT]: {
        messageId: 'dictation_command_copy_selected_text',
        build: RepeatableKeyPress.CopySelectedTextMacro,
      },
      [MacroName.PASTE_TEXT]: {
        messageId: 'dictation_command_paste_text',
        build: RepeatableKeyPress.PasteTextMacro,
      },
      [MacroName.CUT_SELECTED_TEXT]: {
        messageId: 'dictation_command_cut_selected_text',
        build: RepeatableKeyPress.CutSelectedTextMacro,
      },
      [MacroName.UNDO_TEXT_EDIT]: {
        messageId: 'dictation_command_undo_text_edit',
        build: RepeatableKeyPress.UndoTextEditMacro,
      },
      [MacroName.REDO_ACTION]: {
        messageId: 'dictation_command_redo_action',
        build: RepeatableKeyPress.RedoActionMacro,
      },
      [MacroName.SELECT_ALL_TEXT]: {
        messageId: 'dictation_command_select_all_text',
        build: RepeatableKeyPress.SelectAllTextMacro,
      },
      [MacroName.UNSELECT_TEXT]: {
        messageId: 'dictation_command_unselect_text',
        build: RepeatableKeyPress.UnselectTextMacro,
      },
      [MacroName.LIST_COMMANDS]: {
        messageId: 'dictation_command_list_commands',
        build: ListCommandsMacro,
      },
      [MacroName.NEW_LINE]:
          {messageId: 'dictation_command_new_line', build: NewLineMacro},
      [MacroName.TOGGLE_DICTATION]: {
        messageId: 'dictation_command_stop_listening',
        build: ToggleDictationMacro,
      },
      [MacroName.DELETE_PREV_WORD]: {
        messageId: 'dictation_command_delete_prev_word',
        build: RepeatableKeyPress.DeletePrevWordMacro,
      },
      [MacroName.DELETE_PREV_SENT]: {
        messageId: 'dictation_command_delete_prev_sent',
        build: DeletePrevSentMacro,
      },
      [MacroName.NAV_NEXT_WORD]: {
        messageId: 'dictation_command_nav_next_word',
        build: RepeatableKeyPress.NavNextWordMacro,
      },
      [MacroName.NAV_PREV_WORD]: {
        messageId: 'dictation_command_nav_prev_word',
        build: RepeatableKeyPress.NavPrevWordMacro,
      },
      [MacroName.SMART_DELETE_PHRASE]: {
        messageId: 'dictation_command_smart_delete_phrase',
        build: SmartDeletePhraseMacro,
      },
      [MacroName.SMART_REPLACE_PHRASE]: {
        messageId: 'dictation_command_smart_replace_phrase',
        build: SmartReplacePhraseMacro,
      },
      [MacroName.SMART_INSERT_BEFORE]: {
        messageId: 'dictation_command_smart_insert_before',
        build: SmartInsertBeforeMacro,
      },
      [MacroName.SMART_SELECT_BTWN_INCL]: {
        messageId: 'dictation_command_smart_select_btwn_incl',
        build: SmartSelectBetweenMacro,
      },
      [MacroName.NAV_NEXT_SENT]: {
        messageId: 'dictation_command_nav_next_sent',
        build: NavNextSentMacro,
      },
      [MacroName.NAV_PREV_SENT]: {
        messageId: 'dictation_command_nav_prev_sent',
        build: NavPrevSentMacro,
      },
    };
  }
}

/** A parsing strategy that utilizes SimpleMacroFactory. */
export class SimpleParseStrategy extends ParseStrategy {
  /** @param {!InputController} inputController */
  constructor(inputController) {
    super(inputController);

    /**
     * Map of macro names to a factory for that macro.
     * @private {!Map<MacroName, !SimpleMacroFactory>}
     */
    this.macroFactoryMap_ = new Map();

    /** @private {!Set<!MacroName>} */
    this.supportedMacros_ = new Set();

    this.initialize_();
  }

  /** @private */
  initialize_() {
    // Adds all macros that are supported by regular expressions. If a macro
    // has a string associated with it in dictation_strings.grd, then it belongs
    // in this set. Don't add macros that require arguments in their utterances
    // e.g. "select <phrase_or_word>" - these macros are better handled by
    // Pumpkin.
    this.supportedMacros_.add(MacroName.DELETE_PREV_CHAR)
        .add(MacroName.NAV_PREV_CHAR)
        .add(MacroName.NAV_NEXT_CHAR)
        .add(MacroName.NAV_PREV_LINE)
        .add(MacroName.NAV_NEXT_LINE)
        .add(MacroName.COPY_SELECTED_TEXT)
        .add(MacroName.PASTE_TEXT)
        .add(MacroName.CUT_SELECTED_TEXT)
        .add(MacroName.UNDO_TEXT_EDIT)
        .add(MacroName.REDO_ACTION)
        .add(MacroName.SELECT_ALL_TEXT)
        .add(MacroName.UNSELECT_TEXT)
        .add(MacroName.LIST_COMMANDS)
        .add(MacroName.NEW_LINE)
        .add(MacroName.TOGGLE_DICTATION)
        .add(MacroName.DELETE_PREV_WORD)
        .add(MacroName.DELETE_PREV_SENT)
        .add(MacroName.NAV_NEXT_WORD)
        .add(MacroName.NAV_PREV_WORD)
        .add(MacroName.SMART_DELETE_PHRASE)
        .add(MacroName.SMART_REPLACE_PHRASE)
        .add(MacroName.SMART_INSERT_BEFORE)
        .add(MacroName.SMART_SELECT_BTWN_INCL)
        .add(MacroName.NAV_NEXT_SENT)
        .add(MacroName.NAV_PREV_SENT);

    this.supportedMacros_.forEach((name) => {
      this.macroFactoryMap_.set(
          name, new SimpleMacroFactory(name, this.getInputController()));
    });
  }

  /** @override */
  refresh() {
    this.enabled = LocaleInfo.areCommandsSupported();
    if (!this.enabled) {
      return;
    }

    this.macroFactoryMap_ = new Map();
    this.initialize_();
  }

  /** @override */
  async parse(text) {
    const macros = [];
    for (const [name, factory] of this.macroFactoryMap_) {
      const macro = factory.createMacro(text);
      if (macro) {
        macros.push(macro);
      }
    }
    if (macros.length === 1) {
      return macros[0];
    } else if (macros.length === 2) {
      // Pick which macro to use from the list of matched macros.
      // TODO(crbug.com/1288965): Turn this into a disambiguation class as we
      // add more commands. Currently the only ambiguous macro is DELETE_PHRASE
      // which conflicts with other deletion macros. For example, the phrase
      // "Delete the previous word" should be parsed as a DELETE_PREV_WORD
      // instead of SMART_DELETE_PHRASE with phrase "the previous word".
      // Prioritize other deletion macros over SMART_DELETE_PHRASE.
      return macros[0].getName() === MacroName.SMART_DELETE_PHRASE ? macros[1] :
                                                                     macros[0];
    } else if (macros.length > 2) {
      console.warn(`Unexpected ambiguous macros found for text: ${text}.`);
      return macros[0];
    }

    // The command is simply to input the given text.
    // If `text` starts with `type`, then automatically remove it e.g. convert
    // 'Type testing 123' to 'testing 123'.
    const typePrefix =
        chrome.i18n.getMessage('dictation_command_input_text_view');
    if (text.trim().toLowerCase().startsWith(typePrefix)) {
      text = text.toLowerCase().replace(typePrefix, '').trimStart();
    }

    return new InputTextViewMacro(text, this.getInputController());
  }
}
