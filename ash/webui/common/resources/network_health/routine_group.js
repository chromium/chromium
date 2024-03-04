// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for a group of diagnostic routines.
 */

import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './network_health_container.js';

import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {I18nBehavior} from 'chrome://resources/ash/common/i18n_behavior.js';
import {RoutineResult, RoutineVerdict} from 'chrome://resources/mojo/chromeos/services/network_health/public/mojom/network_diagnostics.mojom-webui.js';

import {Icons, Routine} from './network_diagnostics_types.js';
import {getTemplate} from './routine_group.html.js';

Polymer({
  _template: getTemplate(),
  is: 'routine-group',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /**
     * List of routines to display in the group.
     * @type {!Array<!Routine>}
     */
    routines: {
      type: Array,
      value: [],
    },

    /**
     * Localized name for the group of routines.
     * @type {string}
     */
    name: {
      type: String,
      value: '',
    },

    /**
     * Boolean flag if the container is expanded.
     * @type {boolean}
     */
    expanded: {
      type: Boolean,
      value: false,
    },

    /**
     * Boolean flag if any routines in the group are running.
     * @private {boolean}
     */
    running_: {
      type: Boolean,
      computed: 'routinesRunning_(routines.*)',
    },

    /**
     * Boolean flag if icon representing the group result should be shown.
     * @private {boolean}
     */
    showGroupIcon_: {
      type: Boolean,
      computed: 'computeShowGroupIcon_(running_, expanded)',
    },
  },

  /**
   * Helper function to get the icon for a group of routines based on all of
   * their results.
   * @param {!PolymerDeepPropertyChange} routines
   * @return {string}
   * @private
   */
  getGroupIcon_(routines) {
    // Assume that all tests are complete and passing until proven otherwise.
    let complete = true;
    let failed = false;

    for (const routine of /** @type {!Array<!Routine>} */ (routines.base)) {
      if (!routine.result) {
        complete = false;
        continue;
      }

      switch (routine.result.verdict) {
        case RoutineVerdict.kNoProblem:
          continue;
        case RoutineVerdict.kProblem:
          failed = true;
          break;
        case RoutineVerdict.kNotRun:
          complete = false;
          break;
      }
    }

    if (failed) {
      return Icons.TEST_FAILED;
    }
    if (!complete) {
      return Icons.TEST_NOT_RUN;
    }

    return Icons.TEST_PASSED;
  },

  /**
   * Determine if the group routine icon should be showing.
   * @return {boolean}
   * @private
   */
  computeShowGroupIcon_() {
    return !this.running_ && !this.expanded;
  },

  /**
   * Helper function to get the icon for a routine based on the result.
   * @param {!RoutineResult} result
   * @return {string}
   * @private
   */
  getRoutineIcon_(result) {
    if (!result) {
      return Icons.TEST_NOT_RUN;
    }

    switch (result.verdict) {
      case RoutineVerdict.kNoProblem:
        return Icons.TEST_PASSED;
      case RoutineVerdict.kProblem:
        return Icons.TEST_FAILED;
      case RoutineVerdict.kNotRun:
        return Icons.TEST_NOT_RUN;
    }

    return Icons.TEST_NOT_RUN;
  },

  /**
   * Determine if any routines in the group are running.
   * @param {!PolymerDeepPropertyChange} routines
   * @return {boolean}
   * @private
   */
  routinesRunning_(routines) {
    for (const routine of /** @type {!Array<!Routine>} */ (routines.base)) {
      if (routine.running) {
        return true;
      }
    }
    return false;
  },

  /**
   * Helper function to toggle the expanded properties when the routine group
   * is clicked.
   * @private
   */
  onToggleExpanded_() {
    this.set('expanded', !this.expanded);
  },
});
