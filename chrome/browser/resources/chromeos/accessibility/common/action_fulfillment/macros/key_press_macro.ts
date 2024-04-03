// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventGenerator} from '../../event_generator.js';
import {KeyCode} from '../../key_code.js';

import {Macro, MacroError, RunMacroResult} from './macro.js';
import {MacroName} from './macro_names.js';

/**
 * Class that implements a macro to send a synthetic key event.
 */
export class KeyPressMacro extends Macro {
  private runCount_ = 0;
  private key_: KeyCode|undefined;

  /**
   * `macroName` is used to determine the key press type.
   */
  constructor(macroName: MacroName) {
    super(macroName);
    switch (macroName) {
      case MacroName.KEY_PRESS_SPACE:
        this.key_ = KeyCode.SPACE;
        break;
      case MacroName.KEY_PRESS_LEFT:
        this.key_ = KeyCode.LEFT;
        break;
      case MacroName.KEY_PRESS_RIGHT:
        this.key_ = KeyCode.RIGHT;
        break;
      case MacroName.KEY_PRESS_DOWN:
        this.key_ = KeyCode.DOWN;
        break;
      case MacroName.KEY_PRESS_UP:
        this.key_ = KeyCode.UP;
        break;
      default:
        console.error('Macro ' + macroName + ' is not a key press macro.');
    }
  }

  /**
   * The key press macro should be run twice, once at keydown and once at keyup.
   */
  override triggersAtActionStartAndEnd(): boolean {
    return true;
  }

  override run(): RunMacroResult {
    if (!this.key_) {
      return this.createRunMacroResult_(
          /*isSuccess=*/ false, MacroError.INVALID_USER_INTENT);
    }
    if (this.runCount_ === 0) {
      EventGenerator.sendKeyDown(this.key_);
    } else if (this.runCount_ === 1) {
      EventGenerator.sendKeyUp(this.key_);
    } else {
      console.error('Key press macro cannot be run more than twice.');
      return this.createRunMacroResult_(
          /*isSuccess=*/ false, MacroError.INVALID_USER_INTENT);
    }
    this.runCount_++;
    return this.createRunMacroResult_(/*isSuccess=*/ true);
  }
}
