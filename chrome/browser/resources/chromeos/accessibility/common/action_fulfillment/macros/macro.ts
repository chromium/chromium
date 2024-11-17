// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestImportManager} from '../../testing/test_import_manager.js';
import {Context, ContextChecker} from '../context_checker.js';

import {MacroName} from './macro_names.js';

/**
 * Reasons that canTryAction in CheckContextResult might be false.
 * Similar to MacroError::ErrorType in
 * google3/intelligence/dbw/proto/actuator/errors/macro_error.proto, but
 * only with fields used by Chrome OS.
 */
export enum MacroError {
  UNKNOWN,
  // User intent was poorly formed. For example, a numerical field was set
  // to a string value.
  INVALID_USER_INTENT,
  // Returned when the context is invalid for a macro execution,
  // for example selecting the word "cat" when there is no word "cat" in
  // the text area.
  BAD_CONTEXT,
  // Actuation would fail to be successful. For example, the text area might
  // no longer be active, or the action cannot be taken in the given context.
  FAILED_ACTUATION,
}

/**
 * Which direction this macro will move its associated behavior toward,
 * i.e. ON when turning on Dictation or entering scroll mode,
 * OFF when pausing FaceGaze or ending a drag and drop action, or
 * NONE if this macro does not toggle behavior at all.
 */
export enum ToggleDirection {
  NONE = 'none',
  ON = 'on',
  OFF = 'off',
}

/**
 * Results of checking whether the macro is able to execute in the current
 * context or the MacroError if not.
 * |canTryAction| is true if the macro could be executed in the current context.
 * Similar to CheckContextResult in
 * google3/intelligence/dbw/proto/macros/results/check_context_result.proto.
 * TODO(crbug.com/1264544): Information for disambiguation, like a list of
 * matched nodes, could be added here.
 */
export interface CheckContextResult {
  canTryAction: boolean;
  error?: MacroError;
  failedContext?: Context;
}

/**
 * Results of trying to run a macro.
 * Similar to RunMacroResult in
 * google3/intelligence/dbw/proto/macros/results/run_macro_result.proto.
 */
export interface RunMacroResult {
  isSuccess: boolean;
  error?: MacroError;
}

/**
 * An interface for an Accessibility Macro, which can determine if intents are
 * actionable and execute them.
 * @abstract
 */
export class Macro {
  private macroName_: MacroName;
  private checker_: ContextChecker|undefined;

  /** @param macroName The name of this macro. */
  constructor(macroName: MacroName, checker?: ContextChecker) {
    this.macroName_ = macroName;
    this.checker_ = checker;
  }

  /** Gets the description of the macro the user intends to execute. */
  getName(): MacroName {
    return this.macroName_;
  }

  /** Gets the human-readable description of the macro. Useful for debugging. */
  getNameAsString(): string {
    return MacroName[this.macroName_];
  }

  /**
   * Whether this macro should trigger when the action starts and when it ends.
   * For example, a mouse click would trigger a press when a user's action
   * begins and then a release when the action ends.
   */
  triggersAtActionStartAndEnd(): boolean {
    return false;
  }

  /**
   * Whether this macro performs a toggle behavior. For example, toggling on
   * Dictation.
   */
  isToggle(): boolean {
    return false;
  }

  /**
   * Get the toggle direction if this macro has toggle behavior.
   * @return ToggleDirection representing the direction this macro will move its
   *     associated behavior toward when run.
   *    ToggleDirection.NONE if this macro is not a toggle,
   *    ToggleDirection.ON if this macro will turn on its associated behavior,
   *    ToggleDirection.OFF if this macro will turn off its associated behavior.
   */
  getToggleDirection(): ToggleDirection {
    return ToggleDirection.NONE;
  }

  /**
   * Checks whether a macro can attempt to run in the current context.
   * If this macro has several steps, just checks the first step.
   */
  checkContext(): CheckContextResult {
    if (!this.checker_) {
      // Unable to check context.
      return this.createSuccessCheckContextResult_();
    }

    const failedContext = this.checker_.getFailedContext();
    if (!failedContext) {
      return this.createSuccessCheckContextResult_();
    }

    return this.createFailureCheckContextResult_(
        MacroError.BAD_CONTEXT, failedContext);
  }

  /**
   * Attempts to execute a macro in the current context. This base method
   * should be overridden by each subclass.
   */
  run(): RunMacroResult {
    throw new Error(`The run() function must be implemented by each subclass.`);
  }

  /** Protected helper method to create a CheckContextResult with an error. */
  protected createFailureCheckContextResult_(
      error: MacroError, failedContext?: Context): CheckContextResult {
    return {canTryAction: false, error, failedContext};
  }

  /**
   * Protected helper method to create a CheckContextResult representing
   * success.
   */
  protected createSuccessCheckContextResult_(): CheckContextResult {
    return {canTryAction: true};
  }

  /** Protected helper method to create a RunMacroResult. */
  protected createRunMacroResult_(isSuccess: boolean, error?: MacroError):
      RunMacroResult {
    return {isSuccess, error};
  }

  isSmart(): boolean {
    return false;
  }
}

TestImportManager.exportForTesting(
    ['MacroError', MacroError], ['MacroName', MacroName]);
