// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RoutineProperties} from './diagnostics_types.js';
import {ExecutionProgress, ResultStatusItem} from './routine_list_executor.js';
import {getSimpleResult} from './routine_result_entry.js';
import {RoutineResult, RoutineType, StandardRoutineResult} from './system_routine_controller.mojom-webui.js';

function isBlockingRoutine(routineProp: RoutineProperties): boolean {
  return routineProp.blocking;
}

function getNonBlockingRoutines(routines: RoutineProperties[]): RoutineType[] {
  return routines.filter(r => !isBlockingRoutine(r)).map(getRoutine);
}

function getRoutine(routineProp: RoutineProperties): RoutineType {
  return routineProp.routine;
}

/**
 * @fileoverview
 * Used to aggregate individual tests into a shared category (Wi-Fi).
 */
export class RoutineGroup {
  routineProperties: RoutineProperties[];
  nonBlockingRoutines: Set<RoutineType>;
  routines: RoutineType[];
  groupName: string;
  progress: ExecutionProgress;
  failedTest: RoutineType|null = null;
  inWarningState: boolean = false;
  constructor(routines: RoutineProperties[], groupName: string) {
    /**
     * Store routine properties array for calls to |clone|.
     */
    this.routineProperties = this.routineProperties || routines;
    this.nonBlockingRoutines = new Set(getNonBlockingRoutines(routines));
    this.routines = routines.map(getRoutine);
    this.groupName = groupName;
    this.progress = ExecutionProgress.NOT_STARTED;
    /**
     * Used to track the first test failure in the group of tests.
     */
    this.failedTest = null;
    this.inWarningState = false;
  }

  /**
   * Determine overall status of routine group. Report test status as "running"
   * as long as the current routine is not the last routine in the group.
   */
  setStatus(status: ResultStatusItem): void {
    if (status.progress !== ExecutionProgress.COMPLETED) {
      // Prevent 'WARNING' badge from being overwritten by a subsequent routine.
      this.progress = this.inWarningState ? this.progress : status.progress;
      return;
    }

    const isLastRoutine = this.isLastRoutine(status.routine);
    if (status.result && this.testFailed(status.result)) {
      // Prevent 1st failed test from being overwritten.
      this.failedTest = this.failedTest || status.routine;

      const isBlocking = !this.nonBlockingRoutines.has(status.routine);
      this.inWarningState = this.inWarningState || !isBlocking;

      // We've encountered a blocking failure.
      if (this.failedTest && isBlocking) {
        this.progress = ExecutionProgress.COMPLETED;
        return;
      }
    }

    // Set status to "completed" only when all routines in this group are
    // finished running. Otherwise, check if we're in the warning state
    // before setting the progress to running.
    this.progress = isLastRoutine ? ExecutionProgress.COMPLETED :
        this.inWarningState       ? ExecutionProgress.WARNING :
                                    ExecutionProgress.RUNNING;

    return;
  }

  private testFailed(result: RoutineResult): boolean {
    return getSimpleResult(result) === StandardRoutineResult.kTestFailed;
  }

  private isLastRoutine(routine: RoutineType): boolean {
    return routine === this.routines[this.routines.length - 1];
  }

  /**
   * Used to add new routines after initialization for this instance is done.
   * Ex: Routines that are added only when a flag is enabled.
   * */
  addRoutine(routineProps: RoutineProperties): void {
    this.routines.push(routineProps.routine);
    if (!isBlockingRoutine(routineProps)) {
      this.nonBlockingRoutines.add(routineProps.routine);
    }
    this.routineProperties = [...this.routineProperties, routineProps];
  }

  /**
   * Whether we should skip the remaining routines (true when a blocking
   * routine fails) or not.
   */
  hasBlockingFailure(): boolean {
    if (!this.failedTest) {
      return false;
    }

    // Skip remaining tests if this is a blocking test failure and we're not
    // in a warning state.
    return !this.nonBlockingRoutines.has(this.failedTest) &&
        !this.inWarningState;
  }

  /**
   * Clones properties of RoutineGroup and returns new RoutineGroup
   */
  clone(): RoutineGroup {
    const clonedRoutineGroup =
        new RoutineGroup(this.routineProperties, this.groupName);
    clonedRoutineGroup.progress = this.progress;
    clonedRoutineGroup.failedTest = this.failedTest;
    clonedRoutineGroup.inWarningState = this.inWarningState;
    return clonedRoutineGroup;
  }
}