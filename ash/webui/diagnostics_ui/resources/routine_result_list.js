// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_card.js';
import './diagnostics_shared_css.js';
import './routine_result_entry.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {RoutineType} from './diagnostics_types.js';
import {RoutineGroup} from './routine_group.js';
import {ResultStatusItem} from './routine_list_executor.js'

/**
 * @fileoverview
 * 'routine-result-list' shows a list of routine result entries.
 */
Polymer({
  is: 'routine-result-list',

  _template: html`{__html_template__}`,

  properties: {
    /** @private {!Array<RoutineGroup|ResultStatusItem>} */
    results_: {
      type: Array,
      value: () => [],
    },

    /** @type {boolean} */
    hidden: {
      type: Boolean,
      value: false,
    },

    /** @type {boolean} */
    hideVerticalLines: {
      type: Boolean,
      value: false,
    },

    /** @type {boolean} */
    usingRoutineGroups: {
      type: Boolean,
      value: false,
    },
  },

  /**
   * Resets the list and creates a new list with all routines in the unstarted
   * state. Called by the parent RoutineResultSection when the user starts
   * a test run.
   * @param {!Array<RoutineGroup|RoutineType>} routines
   */
  initializeTestRun(routines) {
    this.clearRoutines();
    if (this.usingRoutineGroups) {
      this.set('results_', routines);
    } else {
      this.addRoutines_(routines);
    }
  },

  /**
   * Removes all the routines from the list.
   */
  clearRoutines() {
    this.splice('results_', 0, this.results_.length);
  },

  /**
   * Creates a list of unstarted routines.
   * @param {!Array<!RoutineType>} routines
   */
  addRoutines_(routines) {
    for (let routine of routines) {
      this.push('results_', new ResultStatusItem(routine));
    }
  },

  /**
   * Updates the routine's status in the results_ list.
   * @param {number} index
   * @param {RoutineGroup|ResultStatusItem} status
   * @private
   */
  updateRoutineStatus_(index, status) {
    assert(index < this.results_.length);
    this.splice('results_', index, 1, status);
  },

  /**
   * Receives the callback from RoutineListExecutor whenever the status of a
   * routine changed.
   * @param {!ResultStatusItem} status
   */
  onStatusUpdate(status) {
    assert(this.results_.length > 0);
    this.results_.forEach((result, idx) => {
      if (this.usingRoutineGroups && result.routines.includes(status.routine)) {
        result.setStatus(status);
        this.updateRoutineStatus_(idx, result.clone());
        return;
      }

      if (status.routine === result.routine) {
        this.updateRoutineStatus_(idx, status);
        return;
      }
    })
  },

  /**
   * @protected
   * @param {{path: string, value: (RoutineGroup|ResultStatusItem)}} item
   * @return {boolean}
   */
  shouldHideVerticalLines_({value}) {
    return this.hideVerticalLines ||
        value === this.results_[this.results_.length - 1];
  },

  /** @override */
  created() {},
});
