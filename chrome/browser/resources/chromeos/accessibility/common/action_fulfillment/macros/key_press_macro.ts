// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventGenerator} from '../../event_generator.js';
import {KeyCode} from '../../key_code.js';

import {Macro, MacroError, RunMacroResult} from './macro.js';
import {MacroName} from './macro_names.js';

export interface KeyCombination {
  key: KeyCode;
  modifiers?: chrome.accessibilityPrivate.SyntheticKeyboardModifiers;
}

/** Class that implements a macro to send a synthetic key event. */
export class KeyPressMacro extends Macro {
  private runCount_ = 0;
  private keyCombination_: KeyCombination;

  constructor(macroName: MacroName, keyCombination: KeyCombination) {
    super(macroName);
    this.keyCombination_ = keyCombination;
  }

  /**
   * The key press macro should be run twice, once at keydown and once at keyup.
   */
  override triggersAtActionStartAndEnd(): boolean {
    return true;
  }

  override run(): RunMacroResult {
    const key = this.keyCombination_.key;
    const modifiers = this.keyCombination_.modifiers || {};
    // To avoid the modifiers getting stripped away, do not use rewriters if
    // there are modifiers.
    const useRewriters = this.keyCombination_.modifiers ? false : true;
    if (this.runCount_ === 0) {
      EventGenerator.sendKeyDown(key, modifiers, useRewriters);
    } else if (this.runCount_ === 1) {
      EventGenerator.sendKeyUp(key, modifiers, useRewriters);
    } else {
      console.error('Key press macro cannot be run more than twice.');
      return this.createRunMacroResult_(
          /*isSuccess=*/ false, MacroError.INVALID_USER_INTENT);
    }
    this.runCount_++;
    return this.createRunMacroResult_(/*isSuccess=*/ true);
  }
}
