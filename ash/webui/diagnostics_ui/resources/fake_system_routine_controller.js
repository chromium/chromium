// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';

import {PowerRoutineResult, RoutineResult, RoutineResultInfo, RoutineRunnerInterface, RoutineType, StandardRoutineResult, SystemRoutineControllerInterface} from './diagnostics_types.js';


/**
 * @fileoverview
 * Implements a fake version of the SystemRoutineController mojo interface.
 */

/** @implements {SystemRoutineControllerInterface} */
export class FakeSystemRoutineController {
  constructor() {
    this.methods_ = new FakeMethodResolver();

    /** private !Map<!RoutineType, !RoutineResult> */
    this.routineResults_ = new Map();

    /**
     * Controls the delay resolving routines. By default this is 0 and routines
     * resolve immediately, but still asynchronously.
     * @private {number}
     **/
    this.delayTimeMilliseconds_ = 0;

    /**
     * Holds the resolver for the next routine waiting to be resolved. This
     * will be null if no routines are in progress.
     * @private {?PromiseResolver}
     */
    this.resolver_ = null;

    /**
     * Holds the remote that is called on completion.
     * @private {?RoutineRunnerInterface}
     */
    this.remote_ = null;

    /**
     * Holds the type of the routine currently running.
     * @private {?RoutineType}
     */
    this.routineType_ = null;

    this.registerMethods();
  }

  /*
   * Implements SystemRoutineController.GetSupportedRoutines
   * @return {!Promise<!{routines: !Array<!RoutineType>}>}
   */
  getSupportedRoutines() {
    return this.methods_.resolveMethod('getSupportedRoutines');
  }

  /**
   * Sets the value that will be returned when calling getSupportedRoutines().
   * @param {!Array<!RoutineType>} routines
   */
  setFakeSupportedRoutines(routines) {
    this.methods_.setResult('getSupportedRoutines', {routines: routines});
  }

  /*
   * Implements SystemRoutineController.RunRoutine.
   * @param {!RoutineType} routineType
   * @param {!RoutineRunnerInterface} remoteRunner
   */
  runRoutine(routineType, remoteRunner) {
    this.resolver_ = new PromiseResolver();
    this.remote_ = remoteRunner;
    this.routineType_ = routineType;

    // If there is a positive or zero delay then setup a timer, otherwise
    // the routine will wait until resolveRoutineForTesting() is called.
    if (this.delayTimeMilliseconds_ >= 0) {
      setTimeout(() => {
        this.fireRemoteWithResult_();
      }, this.delayTimeMilliseconds_);
    }
  }

  /**
   * Setup method resolvers.
   */
  registerMethods() {
    this.methods_.register('getSupportedRoutines');
  }

  /**
   *
   * @param {!RoutineType} routineType
   * @param {!StandardRoutineResult} routineResult
   */
  setFakeStandardRoutineResult(routineType, routineResult) {
    this.routineResults_.set(routineType, {simpleResult: routineResult});
  }

  /**
   *
   * @param {!RoutineType} routineType
   * @param {!PowerRoutineResult} routineResult
   */
  setFakePowerRoutineResult(routineType, routineResult) {
    this.routineResults_.set(routineType, {powerResult: routineResult});
  }

  /**
   * Sets how long each routine waits before resolving. Setting to a value
   * >=0 will use a timer to delay resolution automatically. Setting to -1
   * will allow a caller to manually determine when each routine resolves.
   *
   * If set to -1, the caller can call resolveRoutineForTesting() and
   * isRoutineInProgressForTesting() to manually control in unit tests.
   * @param {number} delayMilliseconds
   */
  setDelayTimeInMillisecondsForTesting(delayMilliseconds) {
    assert(delayMilliseconds >= -1);
    this.delayTimeMilliseconds_ = delayMilliseconds;
  }

  /**
   * Returns the pending run routine promise.
   * @return {!Promise}
   */
  getRunRoutinePromiseForTesting() {
    assert(this.resolver_ != null);
    return this.resolver_.promise;
  }

  /**
   * Resolves a routine that is in progress. The delay time must be set to
   * -1 or this method will assert. It will also assert if there is no
   * routine running. Use isRoutineInProgressForTesting() to determine if
   * a routing is currently in progress.
   * @return {!Promise}
   */
  resolveRoutineForTesting() {
    assert(this.delayTimeMilliseconds_ == -1);
    assert(this.resolver_ != null);
    const promise = this.resolver_.promise;

    this.fireRemoteWithResult_();
    return promise;
  }

  /**
   * Returns true if a routine is in progress. The delay time must be -1
   * otherwise this function will assert.
   * @return {boolean}
   */
  isRoutineInProgressForTesting() {
    assert(this.delayTimeMilliseconds_ == -1);
    return this.resolver_ != null;
  }

  /**
   * Returns the expected result for a running routine.
   * @return {!RoutineResultInfo}
   * @private
   */
  getResultInfo_() {
    assert(this.routineType_ != null);
    let result = this.routineResults_.get(this.routineType_);
    if (result == undefined) {
      result = {simpleResult: StandardRoutineResult.kExecutionError};
    }

    const resultInfo = /** @type {!RoutineResultInfo} */ ({
      type: this.routineType_,
      result: result,
    });

    return resultInfo;
  }

  /**
   * Fires the remote callback with the expected result.
   * @private
   */
  fireRemoteWithResult_() {
    this.remote_.onRoutineResult(this.getResultInfo_());

    this.resolver_.resolve();
    this.resolver_ = null;
    this.remote_ = null;
    this.routineType_ = null;
  }
}
