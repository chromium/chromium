// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Context, ContextChecker} from '../context_checker.js';
import {InputController} from '../input_controller.js';

import {Macro, MacroError, RunMacroResult} from './macro.js';
import {MacroName} from './macro_names.js';

/**
 * Implements a macro that inserts a word or phrase before another word or
 * phrase
 */
export class SmartInsertBeforeMacro extends Macro {
  private inputController_: InputController;
  private insertPhrase_: string;
  private beforePhrase_: string;

  constructor(
      inputController: InputController, insertPhrase: string,
      beforePhrase: string) {
    super(
        MacroName.SMART_INSERT_BEFORE,
        new ContextChecker(inputController).add(Context.EMPTY_EDITABLE));
    this.inputController_ = inputController;
    this.insertPhrase_ = insertPhrase;
    this.beforePhrase_ = beforePhrase;
  }

  override run(): RunMacroResult {
    if (!this.inputController_.isActive()) {
      return this.createRunMacroResult_(
          /*isSuccess=*/ false, MacroError.FAILED_ACTUATION);
    }
    this.inputController_.insertBefore(this.insertPhrase_, this.beforePhrase_);
    return this.createRunMacroResult_(/*isSuccess=*/ true);
  }

  override isSmart(): boolean {
    return true;
  }
}
