// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './data_point.js';
import './diagnostics_card.js';
import './diagnostics_shared.css.js';
import './icons.html.js';
import './percent_bar_chart.js';
import './routine_section.js';
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {convertKibToGibDecimalString, convertKibToMib} from './diagnostics_utils.js';
import {getTemplate} from './memory_card.html.js';
import {getSystemDataProvider} from './mojo_interface_provider.js';
import {TestSuiteStatus} from './routine_list_executor.js';
import {MemoryUsage, MemoryUsageObserverReceiver, SystemDataProviderInterface} from './system_data_provider.mojom-webui.js';
import {RoutineType} from './system_routine_controller.mojom-webui.js';

/**
 * @fileoverview
 * 'memory-card' shows information about system memory.
 */

const MemoryCardElementBase = I18nMixin(PolymerElement);

export class MemoryCardElement extends MemoryCardElementBase {
  static get is(): string {
    return 'memory-card';
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      routines: {
        type: Array,
        value: () => [RoutineType.kMemory],
      },

      memoryUsage: {
        type: Object,
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
  private memoryUsage: MemoryUsage;
  private systemDataProvider: SystemDataProviderInterface =
      getSystemDataProvider();
  private memoryUsageObserverReceiver: MemoryUsageObserverReceiver|null = null;

  constructor() {
    super();
    this.observeMemoryUsage();
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
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

  /**
   * Calculates total used memory from MemoryUsage object.
   */
  private getTotalUsedMemory(memoryUsage: MemoryUsage): number {
    return memoryUsage.totalMemoryKib - memoryUsage.availableMemoryKib;
  }

  /**
   * Calculates total available memory from MemoryUsage object.
   */
  protected getAvailableMemory(): string {
    // Note: The storage value is converted to GiB but we still display "GB" to
    // the user since this is the convention memory manufacturers use.
    return loadTimeData.getStringF(
        'memoryAvailable',
        convertKibToGibDecimalString(this.memoryUsage.availableMemoryKib, 2),
        convertKibToGibDecimalString(this.memoryUsage.totalMemoryKib, 2));
  }

  /**
   * Estimate the total runtime in minutes with kMicrosecondsPerByte = 0.2
   * @return Estimate runtime in minutes
   */
  protected getEstimateRuntimeInMinutes(): number {
    // Since this is an estimate, there's no need to be precise with Kib <-> Kb.
    // 300000Kb per minute, based on kMicrosecondsPerByte above.
    return this.memoryUsage ?
        Math.ceil(this.memoryUsage.totalMemoryKib / 300000) :
        0;
  }

  protected getRunTestsAdditionalMessage(): string {
    return convertKibToMib(this.memoryUsage.availableMemoryKib) >= 500 ?
        '' :
        loadTimeData.getString('notEnoughAvailableMemoryMessage');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'memory-card': MemoryCardElement;
  }
}

customElements.define(MemoryCardElement.is, MemoryCardElement);
