// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MacroName} from '/accessibility_common/dictation/macros/macro_names.js';
import {DeletePrevWordMacro} from '/accessibility_common/dictation/macros/repeatable_key_press_macro.js';
import {StopListeningMacro} from '/accessibility_common/dictation/macros/stop_listening_macro.js';

/**
 * Class that manages "hidden" macros e.g. macros that have been fully
 * implemented but haven't been hooked up to the speech parser.
 */
export class HiddenMacroManager {
  /** @param {!InputController} inputController */
  constructor(inputController) {
    this.inputController_ = inputController;
  }

  /** @param {!MacroName} name The macro to run. */
  runMacroForTesting(name) {
    if (!HiddenMacroManager.isHiddenMacro(name)) {
      throw new Error('HiddenMacroManager can only invoke hidden macros.');
    }

    switch (name) {
      case MacroName.STOP_LISTENING:
        new StopListeningMacro().runMacro();
        break;
      case MacroName.DELETE_PREV_WORD:
        new DeletePrevWordMacro().runMacro();
        break;
      default:
        throw new Error(`Unrecognized macro: ${name}`);
    }
  }

  /**
   * @param {!MacroName} name
   * @return {boolean}
   */
  static isHiddenMacro(name) {
    return HiddenMacroManager.HIDDEN_MACROS_.includes(name);
  }
}

/**
 * Includes all hidden macros.
 * @const {!Array<!MacroName>}
 * @private
 */
HiddenMacroManager.HIDDEN_MACROS_ = [
  MacroName.STOP_LISTENING,
  MacroName.DELETE_PREV_WORD,
];
