// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for a group of diagnostic routines.
 */

import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './network_health_container.js';

import {I18nMixin} from '//resources/ash/common/cr_elements/i18n_mixin.js';
import {RoutineResult, RoutineVerdict} from '//resources/mojo/chromeos/services/network_health/public/mojom/network_diagnostics.mojom-webui.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Icons, Routine} from './network_diagnostics_types.js';
import {getTemplate} from './routine_group.html.js';

const RoutineGroupElementBase = I18nMixin(PolymerElement);

export class RoutineGroupElement extends RoutineGroupElementBase {
  static get is() {
    return 'routine-group' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * List of routines to display in the group.
       */
      routines: {
        type: Array,
      },

      /**
       * Localized name for the group of routines.
       */
      name: {
        type: String,
        value: '',
      },

      /**
       * Boolean flag if the container is expanded.
       */
      expanded: {
        type: Boolean,
        value: false,
      },

      /**
       * Boolean flag if any routines in the group are running.
       */
      running_: {
        type: Boolean,
        computed: 'routinesRunning_(routines.*)',
      },

      /**
       * Boolean flag if icon representing the group result should be shown.
       */
      showGroupIcon_: {
        type: Boolean,
        computed: 'computeShowGroupIcon_(running_, expanded)',
      },
    };
  }

  routines: Routine[];
  name: string;
  expanded: boolean;
  private running_: boolean;
  private showGroupIcon_: boolean;

  /**
   * Helper function to get the icon for a group of routines based on all of
   * their results.
   */
  private getGroupIcon_(routines: {base: Routine[]}): string {
    // Assume that all tests are complete and passing until proven otherwise.
    let complete = true;
    let failed = false;

    for (const routine of routines.base) {
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
  }

  /**
   * Determine if the group routine icon should be showing.
   */
  private computeShowGroupIcon_(): boolean {
    return !this.running_ && !this.expanded;
  }

  /**
   * Helper function to get the icon for a routine based on the result.
   */
  private getRoutineIcon_(result: RoutineResult): string {
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
  }

  /**
   * Determine if any routines in the group are running.
   */
  private routinesRunning_(routines: {base: Routine[]}): boolean {
    return routines.base.some(routine => routine.running);
  }

  /**
   * Helper function to toggle the expanded properties when the routine group
   * is clicked.
   */
  private onToggleExpanded_(): void {
    this.set('expanded', !this.expanded);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [RoutineGroupElement.is]: RoutineGroupElement;
  }
}

customElements.define(RoutineGroupElement.is, RoutineGroupElement);
