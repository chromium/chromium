// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {InputController} from '../input_controller.js';

import {Macro, MacroError} from './macro.js';
import {MacroName} from './macro_names.js';

/** Implements a macro that sets selection between two words or phrases. */
export class SmartSelectBetweenMacro extends Macro {
  /**
   * @param {!InputController} inputController
   * @param {string} startPhrase
   * @param {string} endPhrase
   */
  constructor(inputController, startPhrase, endPhrase) {
    super(MacroName.SMART_SELECT_BTWN_INCL);
    /** @private {!InputController} */
    this.inputController_ = inputController;
    /** @private {string} */
    this.startPhrase_ = startPhrase;
    /** @private {string} */
    this.endPhrase_ = endPhrase;
  }

  /** @override */
  checkContext() {
    return this.createSuccessCheckContextResult_(
        /*willImmediatelyDisambiguate=*/ false);
  }

  /** @override */
  run() {
    if (!this.inputController_.isActive()) {
      return this.createRunMacroResult_(
          /*isSuccess=*/ false, MacroError.FAILED_ACTUATION);
    }
    this.inputController_.selectBetween(this.startPhrase_, this.endPhrase_);
    return this.createRunMacroResult_(/*isSuccess=*/ true);
  }

  /** @override */
  isSmart() {
    return true;
  }
}
