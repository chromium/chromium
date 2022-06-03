// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {InputController} from './input_controller.js';
import {InputTextViewMacro, NewLineMacro} from './macros/input_text_view_macro.js';
import {ListCommandsMacro} from './macros/list_commands_macro.js';
import {Macro} from './macros/macro.js';
import {MacroName} from './macros/macro_names.js';
import * as RepeatableKeyPressMacro from './macros/repeatable_key_press_macro.js';

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

    /** @private {InputController} */
    this.inputController_ = inputController;
  }

  /**
   * Parses user text to produce a macro command.
   * @param {string} text The text to parse.
   * @return {Macro}
   */
  parse(text) {
    if (!this.commandsFeatureEnabled_) {
      // Without ExperimentalAccessibilityDictationCommands feature, all
      // text should be input as-is.
      return new InputTextViewMacro(text, this.inputController_);
    }

    // First, see if the text matches a command action.
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
   * Enables commands.
   * @param {string} locale The Dictation recognition locale.
   */
  setCommandsEnabled(locale) {
    this.isRTLLocale_ = SpeechParser.RTLLocales.has(locale);
    this.commandsFeatureEnabled_ = true;

    // Does pre-work to parse commands: gets translated command strings and
    // generates a map of commands to regular expressions that would match them.
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
