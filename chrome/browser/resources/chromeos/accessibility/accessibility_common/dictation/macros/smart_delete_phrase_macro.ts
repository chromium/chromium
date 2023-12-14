// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Context, ContextChecker} from '../context_checker.js';
import {InputController} from '../input_controller.js';

import {Macro, MacroError, RunMacroResult} from './macro.js';
import {MacroName} from './macro_names.js';

/** Implements a macro that deletes a provided word or phrase. */
export class SmartDeletePhraseMacro extends Macro {
  private inputController_: InputController;
  private phrase_: string;

  constructor(inputController: InputController, phrase: string) {
    super(
        MacroName.SMART_DELETE_PHRASE,
        new ContextChecker(inputController).add(Context.EMPTY_EDITABLE));
    this.inputController_ = inputController;
    this.phrase_ = phrase;
  }

  override run(): RunMacroResult {
    if (!this.inputController_.isActive()) {
      return this.createRunMacroResult_(
          /*isSuccess=*/ false, MacroError.FAILED_ACTUATION);
    }
    this.inputController_.deletePhrase(this.phrase_);
    return this.createRunMacroResult_(/*isSuccess=*/ true);
  }

  override isSmart(): boolean {
    return true;
  }
}
