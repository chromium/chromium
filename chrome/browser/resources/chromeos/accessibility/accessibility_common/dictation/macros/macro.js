// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MacroName} from './macro_names.js';

/**
 * Reasons that canTryAction in CheckContextResult might be false.
 * Similar to MacroError::ErrorType in
 * google3/intelligence/dbw/proto/actuator/errors/macro_error.proto, but
 * only with fields used by Chrome OS.
 * @enum {number}
 */
export const MacroError = {
  UNKNOWN: 0,
  // User intent was poorly formed. For example, a numerical field was set
  // to a string value.
  INVALID_USER_INTENT: 1,
  // Returned when the context is invalid for a macro execution,
  // for example selecting the word "cat" when there is no word "cat" in
  // the text area.
  BAD_CONTEXT: 2,
  // Actuation would fail to be successful. For example, the text area might
  // no longer be active, or the action cannot be taken in the given context.
  FAILED_ACTUATION: 3,
};

/**
 * Results of checking whether the macro is able to execute in the current
 * context or the MacroError if not.
 * |canTryAction| is true if the macro could be executed in the current context.
 * |willImmediatelyDisambiguate| is true if the macro would need to disambiguate
 * in order to execute an action. If |canTryAction| is false,
 * |willImmediatelyDisambiguate| may be undefined. Similar to CheckContextResult
 * in google3/intelligence/dbw/proto/macros/results/check_context_result.proto.
 * TODO(crbug.com/1264544): Information for disambiguation, like a list of
 * matched nodes, could be added here.
 * @typedef {{
 *   canTryAction: boolean,
 *   willImmediatelyDisambiguate: (boolean|undefined),
 *   error: (MacroError|undefined),
 * }}
 */
let CheckContextResult;

/**
 * Results of trying to run a macro.
 * Similar to RunMacroResult in
 * google3/intelligence/dbw/proto/macros/results/run_macro_result.proto.
 * @typedef {{
 *   isSuccess: boolean,
 *   error: (MacroError|undefined),
 * }}
 */
let RunMacroResult;

/**
 * An interface for a Dictation Macro, which can determine if intents are
 * actionable and execute them.
 * @abstract
 */
export class Macro {
  /**
   * @param {MacroName} macroName The name of this macro.
   */
  constructor(macroName) {
    /** @private {MacroName} */
    this.macroName_ = macroName;
  }

  /**
   * Gets the description of the macro the user intends to execute.
   * @return {MacroName}
   */
  getName() {
    return this.macroName_;
  }

  /**
   * Gets the human-readable description of the macro. Useful for debugging.
   * @return {string}
   */
  getNameAsString() {
    const name =
        Object.keys(MacroName).find(key => MacroName[key] === this.macroName_);
    return name ? name : 'UNKNOWN';
  }

  /**
   * Checks whether a macro can attempt to run in the current context.
   * If this macro has several steps, just checks the first step.
   * @return {CheckContextResult}
   * @abstract
   */
  checkContext() {}

  /**
   * Attempts to execute a macro in the current context.
   * @returns {RunMacroResult}
   * @abstract
   */
  run() {}

  /**
   * Protected helper method to create a CheckContextResult with an error.
   * @param {MacroError} error
   * @return {CheckContextResult}
   * @protected
   */
  createFailureCheckContextResult_(error) {
    return {canTryAction: false, error};
  }

  /**
   * Protected helper method to create a CheckContextResult representing
   * success.
   * @param {boolean} willImmediatelyDisambiguate
   * @return {CheckContextResult}
   * @protected
   */
  createSuccessCheckContextResult_(willImmediatelyDisambiguate) {
    return {canTryAction: true, willImmediatelyDisambiguate};
  }

  /**
   * Protected helper method to create a RunMacroResult.
   * @param {boolean} isSuccess
   * @param {MacroError=} error
   * @return {RunMacroResult}
   * @protected
   */
  createRunMacroResult_(isSuccess, error) {
    return {isSuccess, error};
  }

  /** @return {boolean} */
  isSmart() {
    return false;
  }
}
