// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines a simple strategy for parsing text and converting
 * it into a Macro.
 */

import {InputController} from '/accessibility_common/dictation/input_controller.js';
import {InputTextViewMacro, NewLineMacro} from '/accessibility_common/dictation/macros/input_text_view_macro.js';
import {ListCommandsMacro} from '/accessibility_common/dictation/macros/list_commands_macro.js';
import {Macro} from '/accessibility_common/dictation/macros/macro.js';
import {MacroName} from '/accessibility_common/dictation/macros/macro_names.js';
import * as RepeatableKeyPressMacro from '/accessibility_common/dictation/macros/repeatable_key_press_macro.js';
import {ParseStrategy} from '/accessibility_common/dictation/parse/parse_strategy.js';

/**
 * @typedef {{
 *   messageId: string,
 *   build: Function,
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
   * @param {boolean} isRTLLocale
   */
  constructor(macroName, inputController, isRTLLocale) {
    /** @private {!MacroName} */
    this.macroName_ = macroName;
    /** @private {!InputController} */
    this.inputController_ = inputController;
    /** @private {boolean} */
    this.isRTLLocale_ = isRTLLocale;

    if (!SimpleMacroFactory.getData_()[this.macroName_]) {
      throw new Error(
          'Macro is not supported by SimpleMacroFactory: ' + this.macroName_);
    }

    /** @private {string} */
    this.commandString_ = chrome.i18n.getMessage(
        SimpleMacroFactory.getData_()[this.macroName_].messageId);
  }

  /** @return {Macro} */
  createMacro() {
    const args = [];
    switch (this.macroName_) {
      case MacroName.NAV_PREV_CHAR:
      case MacroName.NAV_NEXT_CHAR:
      case MacroName.UNSELECT_TEXT:
        args.push(this.isRTLLocale_);
        break;
      case MacroName.NEW_LINE:
        args.push(this.inputController_);
        break;
    }

    const data = SimpleMacroFactory.getData_();
    return new data[this.macroName_].build(...args);
  }

  /**
   * Checks whether a string matches `commandString_`, ignoring case and
   * whitespace.
   * @param {string} text
   * @return {boolean}
   */
  matchesMacro(text) {
    return text.trim().toLowerCase() === this.commandString_;
  }

  /**
   * @return {Object<MacroName, MacroData>}
   * @private
   */
  static getData_() {
    return {
      [MacroName.DELETE_PREV_CHAR]: {
        messageId: 'dictation_command_delete_prev_char',
        build: RepeatableKeyPressMacro.DeletePreviousCharacterMacro
      },
      [MacroName.NAV_PREV_CHAR]: {
        messageId: 'dictation_command_nav_prev_char',
        build: RepeatableKeyPressMacro.NavPreviousCharMacro
      },
      [MacroName.NAV_NEXT_CHAR]: {
        messageId: 'dictation_command_nav_next_char',
        build: RepeatableKeyPressMacro.NavNextCharMacro
      },
      [MacroName.NAV_PREV_LINE]: {
        messageId: 'dictation_command_nav_prev_line',
        build: RepeatableKeyPressMacro.NavPreviousLineMacro
      },
      [MacroName.NAV_NEXT_LINE]: {
        messageId: 'dictation_command_nav_next_line',
        build: RepeatableKeyPressMacro.NavNextLineMacro
      },
      [MacroName.COPY_SELECTED_TEXT]: {
        messageId: 'dictation_command_copy_selected_text',
        build: RepeatableKeyPressMacro.CopySelectedTextMacro
      },
      [MacroName.PASTE_TEXT]: {
        messageId: 'dictation_command_paste_text',
        build: RepeatableKeyPressMacro.PasteTextMacro
      },
      [MacroName.CUT_SELECTED_TEXT]: {
        messageId: 'dictation_command_cut_selected_text',
        build: RepeatableKeyPressMacro.CutSelectedTextMacro
      },
      [MacroName.UNDO_TEXT_EDIT]: {
        messageId: 'dictation_command_undo_text_edit',
        build: RepeatableKeyPressMacro.UndoTextEditMacro
      },
      [MacroName.REDO_ACTION]: {
        messageId: 'dictation_command_redo_action',
        build: RepeatableKeyPressMacro.RedoActionMacro
      },
      [MacroName.SELECT_ALL_TEXT]: {
        messageId: 'dictation_command_select_all_text',
        build: RepeatableKeyPressMacro.SelectAllTextMacro
      },
      [MacroName.UNSELECT_TEXT]: {
        messageId: 'dictation_command_unselect_text',
        build: RepeatableKeyPressMacro.UnselectTextMacro
      },
      [MacroName.LIST_COMMANDS]: {
        messageId: 'dictation_command_list_commands',
        build: ListCommandsMacro
      },
      [MacroName.NEW_LINE]:
          {messageId: 'dictation_command_new_line', build: NewLineMacro},
    };
  }
}

/** A parsing strategy that utilizes SimpleMacroFactory. */
export class SimpleParseStrategy extends ParseStrategy {
  /**
   * @param {!InputController} inputController
   * @param {boolean} isRTLLocale
   */
  constructor(inputController, isRTLLocale) {
    super(inputController, isRTLLocale);

    /**
     * Map of macro names to a factory for that macro.
     * @private {!Map<MacroName, !SimpleMacroFactory>}
     */
    this.macroFactoryMap_ = new Map();

    this.initialize_();
  }

  /** @private */
  initialize_() {
    for (const key in MacroName) {
      const name = MacroName[key];
      if (name === MacroName.INPUT_TEXT_VIEW || name === MacroName.UNSPECIFID) {
        continue;
      }

      this.macroFactoryMap_.set(
          name,
          new SimpleMacroFactory(
              name, this.getInputController(), this.getIsRTLLocale()));
    }
  }

  /** @override */
  async parse(text) {
    for (const [name, factory] of this.macroFactoryMap_) {
      if (factory.matchesMacro(text)) {
        return factory.createMacro();
      }
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
