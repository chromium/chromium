// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {InputController} from '../input_controller.js';

import {Macro, MacroError} from './macro.js';
import {MacroName} from './macro_names.js';

/**
 * Implements a macro that replaces a word or phrase with another word or
 * phrase
 */
export class SmartReplacePhraseMacro extends Macro {
  /**
   * @param {!InputController} inputController
   * @param {string} deletePhrase
   * @param {string} insertPhrase
   */
  constructor(inputController, deletePhrase, insertPhrase) {
    super(MacroName.SMART_REPLACE_PHRASE);
    /** @private {!InputController} */
    this.inputController_ = inputController;
    /** @private {string} */
    this.deletePhrase_ = deletePhrase;
    /** @private {string} */
    this.insertPhrase_ = insertPhrase;
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
    this.inputController_.replacePhrase(this.deletePhrase_, this.insertPhrase_);
    return this.createRunMacroResult_(/*isSuccess=*/ true);
  }

  /** @override */
  isSmart() {
    return true;
  }
}
