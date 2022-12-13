// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './data_point.js';
import './diagnostics_card.js';
import './diagnostics_shared.css.js';
import './icons.html.js';
import './realtime_cpu_chart.js';
import './routine_section.js';
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cpu_card.html.js';
import {getSystemDataProvider} from './mojo_interface_provider.js';
import {TestSuiteStatus} from './routine_list_executor.js';
import {CpuUsage, CpuUsageObserverReceiver, SystemDataProviderInterface, SystemInfo} from './system_data_provider.mojom-webui.js';
import {RoutineType} from './system_routine_controller.mojom-webui.js';

/**
 * @fileoverview
 * 'cpu-card' shows information about the CPU.
 */

const CpuCardElementBase = I18nMixin(PolymerElement);

export class CpuCardElement extends CpuCardElementBase {
  static get is() {
    return 'cpu-card';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
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

      cpuUsage_: {
        type: Object,
      },

      cpuChipInfo_: {
        type: String,
        value: '',
      },

      testSuiteStatus: {
        type: Number,
        value: TestSuiteStatus.NOT_RUNNING,
        notify: true,
      },

      isActive: {
        type: Boolean,
      },

    };
  }

  testSuiteStatus: TestSuiteStatus;
  isActive: boolean;
  private routines_: RoutineType[];
  private cpuUsage_: CpuUsage;
  private cpuChipInfo_: string;
  private systemDataProvider_: SystemDataProviderInterface =
      getSystemDataProvider();
  private cpuUsageObserverReceiver_: CpuUsageObserverReceiver|null = null;

  constructor() {
    super();
    this.observeCpuUsage_();
    this.fetchSystemInfo_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    if (this.cpuUsageObserverReceiver_) {
      this.cpuUsageObserverReceiver_.$.close();
    }
  }

  /** @private */
  private observeCpuUsage_(): void {
    this.cpuUsageObserverReceiver_ = new CpuUsageObserverReceiver(this);

    this.systemDataProvider_.observeCpuUsage(
        this.cpuUsageObserverReceiver_.$.bindNewPipeAndPassRemote());
  }

  /**
   * Implements CpuUsageObserver.onCpuUsageUpdated.
   */
  onCpuUsageUpdated(cpuUsage: CpuUsage): void {
    this.cpuUsage_ = cpuUsage;
  }

  protected getCurrentlyUsing_(): string {
    const MAX_PERCENTAGE = 100;
    const usagePercentage = Math.min(
        (this.cpuUsage_.percentUsageSystem + this.cpuUsage_.percentUsageUser),
        MAX_PERCENTAGE);
    return loadTimeData.getStringF('cpuUsageText', usagePercentage);
  }

  private fetchSystemInfo_(): void {
    this.systemDataProvider_.getSystemInfo().then(
        (result: {systemInfo: SystemInfo}) => {
          this.onSystemInfoReceived_(result.systemInfo);
        });
  }

  private onSystemInfoReceived_(systemInfo: SystemInfo): void {
    this.cpuChipInfo_ = loadTimeData.getStringF(
        'cpuChipText', systemInfo.cpuModelName, systemInfo.cpuThreadsCount,
        this.convertKhzToGhz_(systemInfo.cpuMaxClockSpeedKhz));
  }

  protected getCpuTemp_(): string {
    return loadTimeData.getStringF(
        'cpuTempText', this.cpuUsage_.averageCpuTempCelsius);
  }

  protected getCpuUsageTooltipText_(): string {
    return loadTimeData.getString('cpuUsageTooltipText');
  }

  private convertKhzToGhz_(num: number): string {
    return (num / 1000000).toFixed(2);
  }

  protected getCurrentCpuSpeed_(): string {
    return loadTimeData.getStringF(
        'currentCpuSpeedText',
        this.convertKhzToGhz_(this.cpuUsage_.scalingCurrentFrequencyKhz));
  }

  protected getEstimateRuntimeInMinutes_(): number {
    // Each routine runs for a minute
    return this.routines_.length;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cpu-card': CpuCardElement;
  }
}

customElements.define(CpuCardElement.is, CpuCardElement);
