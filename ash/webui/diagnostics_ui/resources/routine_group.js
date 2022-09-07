// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RoutineProperties, RoutineResult, RoutineType, StandardRoutineResult} from './diagnostics_types.js';
import {ExecutionProgress, ResultStatusItem} from './routine_list_executor.js';
import {getSimpleResult} from './routine_result_entry.js';

/**
 * @param {!RoutineProperties} routineProp
 * @return {boolean}
 */
function isBlockingRoutine(routineProp) {
  return routineProp.blocking;
}

/**
 * @param {!Array<!RoutineProperties>} routines
 * @return {!Array<!RoutineType>}
 */
function getNonBlockingRoutines(routines) {
  return routines.filter(r => !isBlockingRoutine(r)).map(getRoutine);
}

/**
 * @param {!RoutineProperties} routineProp
 * @return {!RoutineType}
 */
function getRoutine(routineProp) {
  return routineProp.routine;
}

/**
 * @fileoverview
 * Used to aggregate individual tests into a shared category (Wi-Fi).
 */
export class RoutineGroup {
  /**
   * @param {!Array<!RoutineProperties>} routines
   * @param {string} groupName
   */
  constructor(routines, groupName) {
    /**
     * Store routine properties array for calls to |clone|.
     *  @type {!Array<!RoutineProperties>}
     */
    this.routineProperties = this.routineProperties || routines;
    /** @type {!Set<!RoutineType>} */
    this.nonBlockingRoutines_ = new Set(getNonBlockingRoutines(routines));
    /** @type {!Array<!RoutineType>} */
    this.routines = routines.map(getRoutine);
    /** @type {string} */
    this.groupName = groupName;
    /** @type {!ExecutionProgress} */
    this.progress = ExecutionProgress.kNotStarted;
    /**
     * Used to track the first test failure in the group of tests.
     * @type {?RoutineType}
     */
    this.failedTest = null;
    this.inWarningState = false;
  }

  /**
   * Determine overall status of routine group. Report test status as "running"
   * as long as the current routine is not the last routine in the group.
   * @param {!ResultStatusItem} status
   */
  setStatus(status) {
    if (status.progress !== ExecutionProgress.kCompleted) {
      // Prevent 'WARNING' badge from being overwritten by a subsequent routine.
      this.progress = this.inWarningState ? this.progress : status.progress;
      return;
    }

    const isLastRoutine = this.isLastRoutine_(status.routine);
    if (status.result && this.testFailed_(status.result)) {
      // Prevent 1st failed test from being overwritten.
      this.failedTest = this.failedTest || status.routine;

      const isBlocking = !this.nonBlockingRoutines_.has(status.routine);
      this.inWarningState = this.inWarningState || !isBlocking;

      // We've encountered a blocking failure.
      if (this.failedTest && isBlocking) {
        this.progress = ExecutionProgress.kCompleted;
        return;
      }
    }

    // Set status to "completed" only when all routines in this group are
    // finished running. Otherwise, check if we're in the warning state
    // before setting the progress to running.
    this.progress = isLastRoutine ?
        ExecutionProgress.kCompleted :
        this.inWarningState ? ExecutionProgress.kWarning :
                              ExecutionProgress.kRunning;

    return;
  }

  /**
   * @private
   * @param {!RoutineResult} result
   * @return {boolean}
   */
  testFailed_(result) {
    return getSimpleResult(result) === StandardRoutineResult.kTestFailed;
  }

  /**
   * @private
   * @param {!RoutineType} routine
   * @return {boolean}
   */
  isLastRoutine_(routine) {
    return routine === this.routines[this.routines.length - 1];
  }

  /**
   * Used to add new routines after initialization for this instance is done.
   * Ex: Routines that are added only when a flag is enabled.
   * @param {!RoutineProperties} routineProps
   * */
  addRoutine(routineProps) {
    this.routines.push(routineProps.routine);
    if (!isBlockingRoutine(routineProps)) {
      this.nonBlockingRoutines_.add(routineProps.routine);
    }
    this.routineProperties = [...this.routineProperties, routineProps];
  }

  /**
   * Whether we should skip the remaining routines (true when a blocking
   * routine fails) or not.
   * @return {boolean}
   */
  hasBlockingFailure() {
    if (!this.failedTest) {
      return false;
    }

    // Skip remaining tests if this is a blocking test failure and we're not
    // in a warning state.
    return !this.nonBlockingRoutines_.has(this.failedTest) &&
        !this.inWarningState;
  }

  /**
   * Clones properties of RoutineGroup and returns new RoutineGroup
   * @return {!RoutineGroup}
   */
  clone() {
    const clonedRoutineGroup =
        new RoutineGroup(this.routineProperties, this.groupName);
    clonedRoutineGroup.progress = this.progress;
    clonedRoutineGroup.failedTest = this.failedTest;
    clonedRoutineGroup.inWarningState = this.inWarningState;
    return clonedRoutineGroup;
  }
}