// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Macro} from '/accessibility_common/dictation/macros/macro.js';
import {MacroName} from '/accessibility_common/dictation/macros/macro_names.js';

/** Implements a macro that deletes a provided word or phrase. */
export class SmartDeletePhraseMacro extends Macro {
  /**
   * @param {!InputController} inputController
   * @param {string} phrase
   */
  constructor(inputController, phrase) {
    super(MacroName.SMART_DELETE_PHRASE);
    /** @private {!InputController} */
    this.inputController_ = inputController;
    /** @private {string} */
    this.phrase_ = phrase;
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
    this.inputController_.smartDeletePhrase(this.phrase_);
    return this.createRunMacroResult_(/*isSuccess=*/ true);
  }
}
