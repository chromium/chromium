// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './data_point.js';
import './diagnostics_card.js';
import './diagnostics_shared.css.js';
import './icons.html.js';
import './realtime_cpu_chart.js';
import './routine_section.js';
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cpu_card.html.js';
import {convertKibToMib} from './diagnostics_utils.js';
import {getSystemDataProvider} from './mojo_interface_provider.js';
import {TestSuiteStatus} from './routine_list_executor.js';
import {CpuUsage, CpuUsageObserverReceiver, MemoryUsage, MemoryUsageObserverReceiver, SystemDataProviderInterface, SystemInfo} from './system_data_provider.mojom-webui.js';
import {RoutineType} from './system_routine_controller.mojom-webui.js';

/**
 * @fileoverview
 * 'cpu-card' shows information about the CPU.
 */

const CpuCardElementBase = I18nMixin(PolymerElement);

export class CpuCardElement extends CpuCardElementBase {
  static get is(): string {
    return 'cpu-card';
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      routines: {
        type: Array,
        value: () =>
            [RoutineType.kCpuStress,
             RoutineType.kCpuCache,
             RoutineType.kCpuFloatingPoint,
             RoutineType.kCpuPrime,
    ],
      },

      cpuUsage: {
        type: Object,
      },

      cpuChipInfo: {
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
  private routines: RoutineType[];
  private cpuUsage: CpuUsage;
  private cpuChipInfo: string;
  private memoryUsage: MemoryUsage;
  private systemDataProvider: SystemDataProviderInterface =
      getSystemDataProvider();
  private cpuUsageObserverReceiver: CpuUsageObserverReceiver|null = null;
  private memoryUsageObserverReceiver: MemoryUsageObserverReceiver|null = null;

  constructor() {
    super();
    this.observeCpuUsage();
    this.observeMemoryUsage();
    this.fetchSystemInfo();
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();

    if (this.cpuUsageObserverReceiver) {
      this.cpuUsageObserverReceiver.$.close();
    }

    if (this.memoryUsageObserverReceiver) {
      this.memoryUsageObserverReceiver.$.close();
    }
  }

  private observeMemoryUsage(): void {
    this.memoryUsageObserverReceiver = new MemoryUsageObserverReceiver(this);

    this.systemDataProvider.observeMemoryUsage(
        this.memoryUsageObserverReceiver.$.bindNewPipeAndPassRemote());
  }

  /**
   * Implements MemoryUsageObserver.onMemoryUsageUpdated()
   */
  onMemoryUsageUpdated(memoryUsage: MemoryUsage): void {
    this.memoryUsage = memoryUsage;
  }

  /** @private */
  private observeCpuUsage(): void {
    this.cpuUsageObserverReceiver = new CpuUsageObserverReceiver(this);

    this.systemDataProvider.observeCpuUsage(
        this.cpuUsageObserverReceiver.$.bindNewPipeAndPassRemote());
  }

  /**
   * Implements CpuUsageObserver.onCpuUsageUpdated.
   */
  onCpuUsageUpdated(cpuUsage: CpuUsage): void {
    this.cpuUsage = cpuUsage;
  }

  protected getCurrentlyUsing(): string {
    const MAX_PERCENTAGE = 100;
    const usagePercentage = Math.min(
        (this.cpuUsage.percentUsageSystem + this.cpuUsage.percentUsageUser),
        MAX_PERCENTAGE);
    return loadTimeData.getStringF('cpuUsageText', usagePercentage);
  }

  private fetchSystemInfo(): void {
    this.systemDataProvider.getSystemInfo().then(
        (result: {systemInfo: SystemInfo}) => {
          this.onSystemInfoReceived(result.systemInfo);
        });
  }

  private onSystemInfoReceived(systemInfo: SystemInfo): void {
    this.cpuChipInfo = loadTimeData.getStringF(
        'cpuChipText', systemInfo.cpuModelName, systemInfo.cpuThreadsCount,
        this.convertKhzToGhz(systemInfo.cpuMaxClockSpeedKhz));
  }

  protected getCpuTemp(): string {
    return loadTimeData.getStringF(
        'cpuTempText', this.cpuUsage.averageCpuTempCelsius);
  }

  protected getCpuUsageTooltipText(): string {
    return loadTimeData.getString('cpuUsageTooltipText');
  }

  private convertKhzToGhz(num: number): string {
    return (num / 1000000).toFixed(2);
  }

  protected getCurrentCpuSpeed(): string {
    return loadTimeData.getStringF(
        'currentCpuSpeedText',
        this.convertKhzToGhz(this.cpuUsage.scalingCurrentFrequencyKhz));
  }

  protected getEstimateRuntimeInMinutes(): number {
    // Each routine runs for a minute
    return this.routines.length;
  }

  protected getRunTestsAdditionalMessage(): string {
    return convertKibToMib(this.memoryUsage.availableMemoryKib) >= 625 ?
        '' :
        loadTimeData.getString('notEnoughAvailableMemoryCpuMessage');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cpu-card': CpuCardElement;
  }
}

customElements.define(CpuCardElement.is, CpuCardElement);
