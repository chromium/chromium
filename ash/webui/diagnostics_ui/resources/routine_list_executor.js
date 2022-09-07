// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';

import {RoutineResult, RoutineResultInfo, RoutineRunnerInterface, RoutineRunnerReceiver, RoutineType, SystemRoutineControllerInterface} from './diagnostics_types.js';

/**
 * Represents the execution progress of a test routine.
 * @enum {number}
 */
export const ExecutionProgress = {
  kNotStarted: 0,
  kRunning: 1,
  kCompleted: 2,
  kCancelled: 3,
  kSkipped: 4,
  kWarning: 5,
};

/**
 * Represents the status of the test suite.
 * @enum {number}
 */
export const TestSuiteStatus = {
  kNotRunning: 0,
  kRunning: 1,
  kCompleted: 2,
};

/**
 * Represents the input to a single routine-result-entry in a
 * routine-result-list.
 */
export class ResultStatusItem {
  constructor(routine, progress = ExecutionProgress.kNotStarted) {
    /** @type {!RoutineType} */
    this.routine = routine;

    /** @type {!ExecutionProgress} */
    this.progress = progress;

    /** @type {?RoutineResult} */
    this.result = null;
  }
}

/**
 * The type of the status callback function.
 * @typedef {!function(!ResultStatusItem)}
 */
export let StatusCallbackFunction;

/**
 * Implements the RoutineRunnerInterface remote. Creates a resolver and resolves
 * it when the onRoutineResult function is called.
 */
class ExecutionContext {
  constructor() {
    /** @private {!PromiseResolver} */
    this.resolver_ = new PromiseResolver();

    this.routineRunner = new RoutineRunnerReceiver(
        /** @type {!RoutineRunnerInterface} */ (this));
  }

  /**
   * Implements RoutineRunnerInterface.onRoutineResult.
   * @param {!RoutineResultInfo} result
   **/
  onRoutineResult(result) {
    this.resolver_.resolve(result);
    this.close();
  }

  whenComplete() {
    return this.resolver_.promise;
  }

  close() {
    this.routineRunner.$.close();
  }

  cancel() {
    this.resolver_.resolve(null);
  }
}

/**
 * Executes a list of test routines, firing a status callback with a
 * ResultStatusItem before and after each test. The resulting ResultStatusItem
 * maps directly to an entry in a routine-result-list.
 */
export class RoutineListExecutor {
  /**
   * @param {!SystemRoutineControllerInterface} routineController
   */
  constructor(routineController) {
    /** @private {!SystemRoutineControllerInterface} */
    this.routineController_ = routineController;

    /** @private {?ExecutionContext} */
    this.currentExecutionContext_ = null;

    /** @private {boolean} */
    this.routinesCancelled_ = false;
  }

  /**
   * Executes a list of routines providing a status callback as each test
   * starts and finishes. The return promise will resolve when all tests are
   * completed.
   * @param {!Array<!RoutineType>} routines
   * @param {!function(!ResultStatusItem): void} statusCallback
   * @return {!Promise<!ExecutionProgress>}
   */
  runRoutines(routines, statusCallback) {
    assert(routines.length > 0);

    // Create a chain of promises that each schedule the next routine when
    // they complete, firing the status callback before and after each test.
    let promise = Promise.resolve();
    routines.forEach((name) => {
      promise = promise.then(() => {
        // Notify the status callback of the test status.
        if (this.routinesCancelled_) {
          statusCallback(
              new ResultStatusItem(name, ExecutionProgress.kCancelled));
          return ExecutionProgress.kCancelled;
        }
        statusCallback(new ResultStatusItem(name, ExecutionProgress.kRunning));

        this.currentExecutionContext_ = new ExecutionContext();
        // Create a new remote and execute the next test.
        this.routineController_.runRoutine(
            name,
            this.currentExecutionContext_.routineRunner.$
                .bindNewPipeAndPassRemote());

        // When the test completes, notify the status callback of the
        // result.
        return this.currentExecutionContext_.whenComplete().then((info) => {
          /** @type {!ExecutionProgress} */
          let progress = ExecutionProgress.kCancelled;
          /** @type {?RoutineResultInfo} */
          let result = null;

          if (info !== null) {
            assert(info.type === name);
            progress = ExecutionProgress.kCompleted;
            result = info.result;
          }

          const status = new ResultStatusItem(name, progress);
          status.result = result;
          statusCallback(status);
          return progress;
        });
      });
    });

    return promise;
  }

  close() {
    if (this.currentExecutionContext_) {
      this.currentExecutionContext_.close();
    }
  }

  cancel() {
    if (this.currentExecutionContext_) {
      this.routinesCancelled_ = true;
      this.currentExecutionContext_.cancel();
    }
  }
}
