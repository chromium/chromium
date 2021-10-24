// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './data_point.js';
import './diagnostics_card.js';
import './diagnostics_shared_css.js';
import './icons.js';
import './percent_bar_chart.js';
import './routine_section.js';
import './strings.m.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MemoryUsage, MemoryUsageObserverInterface, MemoryUsageObserverReceiver, RoutineType, SystemDataProviderInterface} from './diagnostics_types.js';
import {convertKibToGibDecimalString, convertKibToMib} from './diagnostics_utils.js';
import {getSystemDataProvider} from './mojo_interface_provider.js';
import {TestSuiteStatus} from './routine_list_executor.js';

/**
 * @fileoverview
 * 'memory-card' shows information about system memory.
 */
Polymer({
  is: 'memory-card',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  /**
   * @private {?SystemDataProviderInterface}
   */
  systemDataProvider_: null,

  /**
   * Receiver responsible for observing memory usage.
   * @private {?MemoryUsageObserverReceiver}
   */
  memoryUsageObserverReceiver_: null,

  properties: {
    /** @private {!Array<!RoutineType>} */
    routines_: {
      type: Array,
      value: () => {
        return [
          RoutineType.kMemory,
        ];
      }
    },

    /** @private {!MemoryUsage} */
    memoryUsage_: {
      type: Object,
    },

    /** @type {!TestSuiteStatus} */
    testSuiteStatus: {
      type: Number,
      value: TestSuiteStatus.kNotRunning,
      notify: true,
    },

    /** @type {boolean} */
    isActive: {
      type: Boolean,
    },
  },

  /** @override */
  created() {
    this.systemDataProvider_ = getSystemDataProvider();
    this.observeMemoryUsage_();
  },

  /** @override */
  detached() {
    this.memoryUsageObserverReceiver_.$.close();
  },

  /** @private */
  observeMemoryUsage_() {
    this.memoryUsageObserverReceiver_ = new MemoryUsageObserverReceiver(
        /**
         * @type {!MemoryUsageObserverInterface}
         */
        (this));

    this.systemDataProvider_.observeMemoryUsage(
        this.memoryUsageObserverReceiver_.$.bindNewPipeAndPassRemote());
  },

  /**
   * Implements MemoryUsageObserver.onMemoryUsageUpdated()
   * @param {!MemoryUsage} memoryUsage
   */
  onMemoryUsageUpdated(memoryUsage) {
    this.memoryUsage_ = memoryUsage;
  },

  /**
   * Calculates total used memory from MemoryUsage object.
   * @param {!MemoryUsage} memoryUsage
   * @return {number}
   * @private
   */
  getTotalUsedMemory_(memoryUsage) {
    return memoryUsage.totalMemoryKib - memoryUsage.availableMemoryKib;
  },

  /**
   * Calculates total available memory from MemoryUsage object.
   * @return {string}
   * @protected
   */
  getAvailableMemory_() {
    // Note: The storage value is converted to GiB but we still display "GB" to
    // the user since this is the convention memory manufacturers use.
    return loadTimeData.getStringF(
        'memoryAvailable',
        convertKibToGibDecimalString(this.memoryUsage_.availableMemoryKib, 2),
        convertKibToGibDecimalString(this.memoryUsage_.totalMemoryKib, 2));
  },

  /**
   * Estimate the total runtime in minutes with kMicrosecondsPerByte = 0.2
   * @return {number} Estimate runtime in minutes
   * @protected
   */
  getEstimateRuntimeInMinutes_() {
    // Since this is an estimate, there's no need to be precise with Kib <-> Kb.
    // 300000Kb per minute, based on kMicrosecondsPerByte above.
    return this.memoryUsage_ ?
        Math.ceil(this.memoryUsage_.totalMemoryKib / 300000) :
        0;
  },

  /**
   * @return {string}
   * @protected
   */
  getRunTestsAdditionalMessage_() {
    return convertKibToMib(this.memoryUsage_.availableMemoryKib) >= 500 ?
        '' :
        loadTimeData.getString('notEnoughAvailableMemoryMessage');
  },
});
