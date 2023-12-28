// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Context, ContextChecker} from '../context_checker.js';
import {InputController} from '../input_controller.js';

import {Macro, MacroError, RunMacroResult} from './macro.js';
import {MacroName} from './macro_names.js';

/** Implements a macro that moves the text caret to the next sentence. */
export class NavNextSentMacro extends Macro {
  private inputController_: InputController;

  constructor(inputController: InputController) {
    super(
        MacroName.NAV_NEXT_SENT,
        new ContextChecker(inputController).add(Context.EMPTY_EDITABLE));
    this.inputController_ = inputController;
  }

  override run(): RunMacroResult {
    if (!this.inputController_.isActive()) {
      return this.createRunMacroResult_(
          /*isSuccess=*/ false, MacroError.FAILED_ACTUATION);
    }
    this.inputController_.navNextSent();
    return this.createRunMacroResult_(/*isSuccess=*/ true);
  }

  override isSmart(): boolean {
    return true;
  }
}

/** Implements a macro that moves the text caret to the previous sentence. */
export class NavPrevSentMacro extends Macro {
  private inputController_: InputController;

  constructor(inputController: InputController) {
    super(
        MacroName.NAV_PREV_SENT,
        new ContextChecker(inputController).add(Context.EMPTY_EDITABLE));
    this.inputController_ = inputController;
  }

  override run(): RunMacroResult {
    if (!this.inputController_.isActive()) {
      return this.createRunMacroResult_(
          /*isSuccess=*/ false, MacroError.FAILED_ACTUATION);
    }
    this.inputController_.navPrevSent();
    return this.createRunMacroResult_(/*isSuccess=*/ true);
  }

  override isSmart(): boolean {
    return true;
  }
}
