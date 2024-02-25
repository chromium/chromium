// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';

import {RoutineResult, RoutineResultInfo, RoutineRunnerReceiver, RoutineType, SystemRoutineControllerInterface} from './system_routine_controller.mojom-webui.js';

/**
 * Represents the execution progress of a test routine.
 */
export enum ExecutionProgress {
  NOT_STARTED,
  RUNNING,
  COMPLETED,
  CANCELLED,
  SKIPPED,
  WARNING,
}

/**
 * Represents the status of the test suite.
 */
export enum TestSuiteStatus {
  NOT_RUNNING,
  RUNNING,
  COMPLETED,
}

/**
 * Represents the input to a single routine-result-entry in a
 * routine-result-list.
 */
export class ResultStatusItem {
  routine: RoutineType;
  progress: ExecutionProgress;
  result: RoutineResult|null = null;

  constructor(routine: RoutineType, progress = ExecutionProgress.NOT_STARTED) {
    this.routine = routine;
    this.progress = progress;
  }
}

/**
 * The type of the status callback function.
 */
export type StatusCallbackFunction = (arg0: ResultStatusItem) => void;

type InitialRunRoutineStatus =
    void|ExecutionProgress.CANCELLED|ExecutionProgress.COMPLETED;
type RunRoutineStatus = Exclude<InitialRunRoutineStatus, void>;


/**
 * Implements the RoutineRunnerInterface remote. Creates a resolver and resolves
 * it when the onRoutineResult function is called.
 */
class ExecutionContext {
  routineRunner: RoutineRunnerReceiver = new RoutineRunnerReceiver(this);
  private resolver: PromiseResolver<RoutineResultInfo|null> =
      new PromiseResolver();

  /**
   * Implements RoutineRunnerInterface.onRoutineResult.
   */
  onRoutineResult(result: RoutineResultInfo): void {
    this.resolver.resolve(result);
    this.close();
  }

  whenComplete(): Promise<RoutineResultInfo|null> {
    return this.resolver.promise;
  }

  close(): void {
    this.routineRunner.$.close();
  }

  cancel(): void {
    this.resolver.resolve(null);
  }
}

/**
 * Executes a list of test routines, firing a status callback with a
 * ResultStatusItem before and after each test. The resulting ResultStatusItem
 * maps directly to an entry in a routine-result-list.
 */
export class RoutineListExecutor {
  private routineController: SystemRoutineControllerInterface;
  private currentExecutionContext: ExecutionContext|null = null;
  private routinesCancelled: boolean = false;
  constructor(routineController: SystemRoutineControllerInterface) {
    this.routineController = routineController;
  }

  /**
   * Executes a list of routines providing a status callback as each test
   * starts and finishes. The return promise will resolve when all tests are
   * completed.
   */
  runRoutines(routines: RoutineType[], statusCallback: StatusCallbackFunction):
      Promise<RunRoutineStatus> {
    assert(routines.length > 0);

    // Create a chain of promises that each schedule the next routine when
    // they complete, firing the status callback before and after each test.
    let promise: Promise<InitialRunRoutineStatus> = Promise.resolve();
    routines.forEach((name) => {
      promise = promise.then(() => {
        // Notify the status callback of the test status.
        if (this.routinesCancelled) {
          statusCallback(
              new ResultStatusItem(name, ExecutionProgress.CANCELLED));
          return ExecutionProgress.CANCELLED;
        }
        statusCallback(new ResultStatusItem(name, ExecutionProgress.RUNNING));

        this.currentExecutionContext = new ExecutionContext();
        // Create a new remote and execute the next test.
        this.routineController.runRoutine(
            name,
            this.currentExecutionContext.routineRunner.$
                .bindNewPipeAndPassRemote());

        // When the test completes, notify the status callback of the
        // result.
        return this.currentExecutionContext.whenComplete().then(
            (info: RoutineResultInfo|null) => {
              let progress = ExecutionProgress.CANCELLED;
              let result = null;

              if (info !== null) {
                assert(info.type === name);
                progress = ExecutionProgress.COMPLETED;
                result = info.result;
              }

              const status = new ResultStatusItem(name, progress);
              status.result = result;
              statusCallback(status);
              return progress;
            });
      });
    });

    return promise as Promise<RunRoutineStatus>;
  }

  close(): void {
    if (this.currentExecutionContext) {
      this.currentExecutionContext.close();
    }
  }

  cancel(): void {
    if (this.currentExecutionContext) {
      this.routinesCancelled = true;
      this.currentExecutionContext.cancel();
    }
  }
}
