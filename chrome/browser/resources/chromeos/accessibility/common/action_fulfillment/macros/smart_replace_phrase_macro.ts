// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Context, ContextChecker} from '../context_checker.js';
import {InputController} from '../input_controller.js';

import {Macro, MacroError, RunMacroResult} from './macro.js';
import {MacroName} from './macro_names.js';

/**
 * Implements a macro that replaces a word or phrase with another word or
 * phrase
 */
export class SmartReplacePhraseMacro extends Macro {
  private inputController_: InputController;
  private deletePhrase_: string;
  private insertPhrase_: string;

  constructor(
      inputController: InputController, deletePhrase: string,
      insertPhrase: string) {
    super(
        MacroName.SMART_REPLACE_PHRASE,
        new ContextChecker(inputController).add(Context.EMPTY_EDITABLE));
    this.inputController_ = inputController;
    this.deletePhrase_ = deletePhrase;
    this.insertPhrase_ = insertPhrase;
  }

  override run(): RunMacroResult {
    if (!this.inputController_.isActive()) {
      return this.createRunMacroResult_(
          /*isSuccess=*/ false, MacroError.FAILED_ACTUATION);
    }
    this.inputController_.replacePhrase(this.deletePhrase_, this.insertPhrase_);
    return this.createRunMacroResult_(/*isSuccess=*/ true);
  }

  override isSmart(): boolean {
    return true;
  }
}
