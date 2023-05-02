// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Context, ContextChecker} from '../context_checker.js';
import {InputController} from '../input_controller.js';

import {Macro, MacroError} from './macro.js';
import {MacroName} from './macro_names.js';

/**
 * Implements a macro that inserts a word or phrase before another word or
 * phrase
 */
export class SmartInsertBeforeMacro extends Macro {
  /**
   * @param {!InputController} inputController
   * @param {string} insertPhrase
   * @param {string} beforePhrase
   */
  constructor(inputController, insertPhrase, beforePhrase) {
    super(
        MacroName.SMART_INSERT_BEFORE,
        new ContextChecker(inputController).add(Context.EMPTY_EDITABLE));
    /** @private {!InputController} */
    this.inputController_ = inputController;
    /** @private {string} */
    this.insertPhrase_ = insertPhrase;
    /** @private {string} */
    this.beforePhrase_ = beforePhrase;
  }

  /** @override */
  run() {
    if (!this.inputController_.isActive()) {
      return this.createRunMacroResult_(
          /*isSuccess=*/ false, MacroError.FAILED_ACTUATION);
    }
    this.inputController_.insertBefore(this.insertPhrase_, this.beforePhrase_);
    return this.createRunMacroResult_(/*isSuccess=*/ true);
  }

  /** @override */
  isSmart() {
    return true;
  }
}
