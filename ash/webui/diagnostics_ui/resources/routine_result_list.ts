// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_card.js';
import './diagnostics_shared.css.js';
import './routine_result_entry.js';

import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {RoutineGroup} from './routine_group.js';
import {ExecutionProgress, ResultStatusItem} from './routine_list_executor.js';
import {getTemplate} from './routine_result_list.html.js';
import {RoutineType} from './system_routine_controller.mojom-webui.js';

export const isRoutineGroupArray =
    (arr: RoutineGroup[]|RoutineType[]): arr is RoutineGroup[] =>
        (arr as RoutineGroup[])[0].groupName !== undefined;
export const isRoutineTypeArray =
    (arr: RoutineGroup[]|RoutineType[]): arr is RoutineType[] =>
        Object.values(RoutineType).includes(arr[0] as RoutineType);

type ResultsType = RoutineGroup[]|ResultStatusItem[];

/**
 * @fileoverview
 * 'routine-result-list' shows a list of routine result entries.
 */

export class RoutineResultListElement extends PolymerElement {
  static get is(): 'routine-result-list' {
    return 'routine-result-list' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      results: {
        type: Array,
        value: () => [],
      },

      hidden: {
        type: Boolean,
        value: false,
      },

      hideVerticalLines: {
        type: Boolean,
        value: false,
      },

      usingRoutineGroups: {
        type: Boolean,
        value: false,
      },

      /**
       * Only used with routine groups.
       */
      ignoreRoutineStatusUpdates: {
        type: Boolean,
        value: false,
      },

    };
  }

  override hidden: boolean;
  hideVerticalLines: boolean;
  usingRoutineGroups: boolean;
  ignoreRoutineStatusUpdates: boolean;
  private results: ResultsType;

  /**
   * Resets the list and creates a new list with all routines in the unstarted
   * state. Called by the parent RoutineResultSection when the user starts
   * a test run.
   */
  initializeTestRun(routines: RoutineGroup[]|RoutineType[]): void {
    this.clearRoutines();
    if (routines.length === 0) {
      return;
    }
    if (this.usingRoutineGroups && isRoutineGroupArray(routines)) {
      this.set('results', routines);
    } else {
      assert(isRoutineTypeArray(routines));
      this.addRoutines(routines);
    }
  }

  /**
   * Removes all the routines from the list.
   */
  clearRoutines(): void {
    this.splice('results', 0, this.results.length);
  }

  /**
   * Creates a list of unstarted routines.
   */
  private addRoutines(routines: RoutineType[]): void {
    for (const routine of routines) {
      this.push('results', new ResultStatusItem(routine));
    }
  }

  /**
   * Updates the routine's status in the results list.
   */
  private updateRoutineStatus(
      index: number, status: RoutineGroup|ResultStatusItem): void {
    assert(index < this.results.length);
    this.splice('results', index, 1, status);
  }

  /**
   * Receives the callback from RoutineListExecutor whenever the status of a
   * routine changed.
   */
  onStatusUpdate(status: ResultStatusItem): void {
    if (this.ignoreRoutineStatusUpdates) {
      return;
    }
    assert(this.results.length > 0);
    this.results.forEach(
        (result: RoutineGroup|ResultStatusItem, idx: number) => {
          if (result instanceof RoutineGroup &&
              result.routines.includes(status.routine)) {
            result.setStatus(status);
            const shouldUpdateRoutineUI = result.hasBlockingFailure();
            this.hideVerticalLines = shouldUpdateRoutineUI;
            this.updateRoutineStatus(idx, result.clone());
            // Whether we should skip the remaining routines (true when a
            // blocking routine fails) or not.
            if (shouldUpdateRoutineUI) {
              this.ignoreRoutineStatusUpdates = true;
              this.updateRoutineUiAfterFailure();
            }
            return;
          }
          if (result instanceof ResultStatusItem) {
            if (status.routine === result.routine) {
              this.updateRoutineStatus(idx, status);
              return;
            }
          }
        });
  }

  protected shouldHideVerticalLines({value}: {
    value: RoutineGroup|ResultStatusItem,
  }): boolean {
    return this.hideVerticalLines ||
        value === this.results[this.results.length - 1];
  }

  /**
   * When a test in a routine group fails, we stop sending status updates to the
   * UI and display 'SKIPPED' for the remaining routine groups.
   */
  updateRoutineUiAfterFailure(): void {
    assert(this.usingRoutineGroups);
    this.results.forEach(
        (routineGroup: RoutineGroup|ResultStatusItem, i: number) => {
          assert(routineGroup instanceof RoutineGroup);
          if (routineGroup.progress === ExecutionProgress.NOT_STARTED) {
            routineGroup.progress = ExecutionProgress.SKIPPED;
            this.updateRoutineStatus(i, routineGroup.clone());
          }
        });
  }

  /**
   * Called from 'routine-section' after all routines have finished running.
   */
  resetIgnoreStatusUpdatesFlag(): void {
    this.ignoreRoutineStatusUpdates = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [RoutineResultListElement.is]: RoutineResultListElement;
  }
}

customElements.define(RoutineResultListElement.is, RoutineResultListElement);
