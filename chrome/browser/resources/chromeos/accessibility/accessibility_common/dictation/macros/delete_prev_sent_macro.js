// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {InputController} from '../input_controller.js';

import {Macro, MacroError} from './macro.js';
import {MacroName} from './macro_names.js';

/** Class that implements a macro that deletes the previous sentence. */
export class DeletePrevSentMacro extends Macro {
  /** @param {!InputController} inputController */
  constructor(inputController) {
    super(MacroName.DELETE_PREV_SENT);

    /** @private {!InputController} */
    this.inputController_ = inputController;
  }

  /** @override */
  checkContext() {
    return this.createSuccessCheckContextResult_(
        /*willImmediatelyDisambiguate=*/ false);
  }

  /** @override */
  runMacro() {
    if (!this.inputController_.isActive()) {
      return this.createRunMacroResult_(
          /*isSuccess=*/ false, MacroError.FAILED_ACTUATION);
    }
    this.inputController_.deletePrevSentence();
    return this.createRunMacroResult_(/*isSuccess=*/ true);
  }

  /** @override */
  isSmart() {
    return true;
  }
}
