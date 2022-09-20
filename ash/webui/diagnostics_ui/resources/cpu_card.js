// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './data_point.js';
import './diagnostics_card.js';
import './diagnostics_shared_css.js';
import './icons.js';
import './realtime_cpu_chart.js';
import './routine_section.js';
import './strings.m.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/cr_elements/i18n_behavior.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getSystemDataProvider} from './mojo_interface_provider.js';
import {TestSuiteStatus} from './routine_list_executor.js';
import {CpuUsage, CpuUsageObserverInterface, CpuUsageObserverReceiver, SystemDataProviderInterface, SystemInfo} from './system_data_provider.mojom-webui.js';
import {RoutineType} from './system_routine_controller.mojom-webui.js';

/**
 * @fileoverview
 * 'cpu-card' shows information about the CPU.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const CpuCardElementBase = mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class CpuCardElement extends CpuCardElementBase {
  static get is() {
    return 'cpu-card';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private {!Array<!RoutineType>} */
      routines_: {
        type: Array,
        value: () => {
          return [
            RoutineType.kCpuStress,
            RoutineType.kCpuCache,
            RoutineType.kCpuFloatingPoint,
            RoutineType.kCpuPrime,
          ];
        },
      },

      /** @private {!CpuUsage} */
      cpuUsage_: {
        type: Object,
      },

      /** @private {string} */
      cpuChipInfo_: {
        type: String,
        value: '',
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

    };
  }

  /** @override */
  constructor() {
    super();
    /**
     * @private {?SystemDataProviderInterface}
     */
    this.systemDataProvider_ = null;

    /**
     * Receiver responsible for observing CPU usage.
     * @private {?CpuUsageObserverReceiver}
     */
    this.cpuUsageObserverReceiver_ = null;

    this.systemDataProvider_ = getSystemDataProvider();
    this.observeCpuUsage_();
    this.fetchSystemInfo_();
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();

    this.cpuUsageObserverReceiver_.$.close();
  }

  /** @private */
  observeCpuUsage_() {
    this.cpuUsageObserverReceiver_ = new CpuUsageObserverReceiver(
        /**
         * @type {!CpuUsageObserverInterface}
         */
        (this));

    this.systemDataProvider_.observeCpuUsage(
        this.cpuUsageObserverReceiver_.$.bindNewPipeAndPassRemote());
  }

  /**
   * Implements CpuUsageObserver.onCpuUsageUpdated.
   * @param {!CpuUsage} cpuUsage
   */
  onCpuUsageUpdated(cpuUsage) {
    this.cpuUsage_ = cpuUsage;
  }

  /** @protected */
  getCurrentlyUsing_() {
    const MAX_PERCENTAGE = 100;
    const usagePercentage = Math.min(
        (this.cpuUsage_.percentUsageSystem + this.cpuUsage_.percentUsageUser),
        MAX_PERCENTAGE);
    return loadTimeData.getStringF('cpuUsageText', usagePercentage);
  }

  /** @private */
  fetchSystemInfo_() {
    this.systemDataProvider_.getSystemInfo().then((result) => {
      this.onSystemInfoReceived_(result.systemInfo);
    });
  }

  /**
   * @param {!SystemInfo} systemInfo
   * @private
   */
  onSystemInfoReceived_(systemInfo) {
    this.cpuChipInfo_ = loadTimeData.getStringF(
        'cpuChipText', systemInfo.cpuModelName, systemInfo.cpuThreadsCount,
        this.convertKhzToGhz_(systemInfo.cpuMaxClockSpeedKhz));
  }

  /** @protected */
  getCpuTemp_() {
    return loadTimeData.getStringF(
        'cpuTempText', this.cpuUsage_.averageCpuTempCelsius);
  }

  /** @protected */
  getCpuUsageTooltipText_() {
    return loadTimeData.getString('cpuUsageTooltipText');
  }

  /**
   * @param {number} num
   * @return {string}
   * @private
   */
  convertKhzToGhz_(num) {
    return parseFloat(num / 1000000).toFixed(2);
  }

  /** @protected */
  getCurrentCpuSpeed_() {
    return loadTimeData.getStringF(
        'currentCpuSpeedText',
        this.convertKhzToGhz_(this.cpuUsage_.scalingCurrentFrequencyKhz));
  }

  /** @protected */
  getEstimateRuntimeInMinutes_() {
    // Each routine runs for a minute
    return this.routines_.length;
  }
}

customElements.define(CpuCardElement.is, CpuCardElement);