// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RoutineType, StandardRoutineResult} from './diagnostics_types.js';
import {ExecutionProgress, ResultStatusItem} from './routine_list_executor.js';
import {getRoutineType, getSimpleResult} from './routine_result_entry.js';

/**
 * @fileoverview
 * Used to aggregate individual tests into a shared category (Wi-Fi).
 */
export class RoutineGroup {
  /**
   * @param {!Array<!RoutineType>} routines
   * @param {string} groupName
   */
  constructor(routines, groupName) {
    /** @type {!Array<!RoutineType>} */
    this.routines = routines;
    /** @type {string} */
    this.groupName = groupName;
    /** @type {!ExecutionProgress} */
    this.progress = ExecutionProgress.kNotStarted;

    /**
     * Used to track the first test failure in the group of tests.
     * @type {?string}
     */
    this.failedTest = null;
  }

  /**
   * Determine overall status of routine group. Report test status as "running"
   * as long as the current routine is not the last routine in the group.
   * @param {!ResultStatusItem} status
   */
  setStatus(status) {
    if (status.progress !== ExecutionProgress.kCompleted) {
      this.progress = status.progress;
      return;
    }

    const isLastRoutine =
        status.routine === this.routines[this.routines.length - 1];

    // Only set status to "completed" if all routines in this group
    // are finished running.
    this.progress = isLastRoutine ? ExecutionProgress.kCompleted :
                                    ExecutionProgress.kRunning;

    const testFailed =
        getSimpleResult(/** @type {!ResultStatusItem} */ (status.result)) !==
        StandardRoutineResult.kTestPassed;
    if (status.result && testFailed) {
      this.failedTest = this.failedTest || getRoutineType(status.routine);
    }
  }

  /**
   * Clones properties of RoutineGroup and returns new RoutineGroup
   * @return {!RoutineGroup}
   */
  clone() {
    const clonedRoutineGroup = new RoutineGroup(this.routines, this.groupName);
    clonedRoutineGroup.progress = this.progress;
    clonedRoutineGroup.failedTest = this.failedTest;
    return clonedRoutineGroup;
  }
}