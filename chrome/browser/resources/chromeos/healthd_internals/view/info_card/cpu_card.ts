// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './info_card.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {CpuUsage} from '../../model/cpu_usage_helper.js';
import {getAverageCpuUsage} from '../../utils/cpu_usage_utils.js';
import type {HealthdApiPhysicalCpuResult, HealthdApiTelemetryResult} from '../../utils/externs.js';
import {toFixedFloat} from '../../utils/number_utils.js';

import {getTemplate} from './cpu_card.html.js';
import type {HealthdInternalsInfoCardElement} from './info_card.js';

export interface HealthdInternalsCpuCardElement {
  $: {
    infoCard: HealthdInternalsInfoCardElement,
  };
}

export class HealthdInternalsCpuCardElement extends PolymerElement {
  static get is() {
    return 'healthd-internals-cpu-card';
  }

  static get template() {
    return getTemplate();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.$.infoCard.appendCardRow('INFO', true);
    this.$.infoCard.appendCardRow('USAGE', true);

    this.$.infoCard.updateDisplayedInfo(1, {
      'Overall': '0.00%',
      'System': '0.00%',
      'User': '0.00%',
      'Idle': '0.00%',
    });
  }

  // Whether the rows of physical CPUs are initialized.
  private isInitialized: boolean = false;

  updateTelemetryData(data: HealthdApiTelemetryResult) {
    this.$.infoCard.updateDisplayedInfo(0, {
      'Number of Cores': parseInt(data.cpu.numTotalThreads),
      'Architecture': data.cpu.architecture,
    });

    const physicalCpus: HealthdApiPhysicalCpuResult[] = data.cpu.physicalCpus;
    if (!this.isInitialized) {
      for (let i: number = 0; i < physicalCpus.length; ++i) {
        this.$.infoCard.appendCardRow(`PHYSCICAL CPU #${i}`);
      }
      this.$.infoCard.refreshComponents();
      this.isInitialized = true;
    }

    const nextIdx: number = 2;
    for (let i: number = 0; i < physicalCpus.length; ++i) {
      this.$.infoCard.updateDisplayedInfo(nextIdx + i, {
        'Model': physicalCpus[i].modelName,
        'Logical CPUs': physicalCpus[i].logicalCpus.map((logicalCpu) => {
          const curFreqKhz = parseInt(logicalCpu.frequency.current);
          const maxFreqKhz = parseInt(logicalCpu.frequency.max);
          const freqPercentage = (maxFreqKhz === 0) ?
              'N/A' :
              toFixedFloat(curFreqKhz / maxFreqKhz * 100, 2);
          return {
            'Core ID': logicalCpu.coreId,
            'Current / Max Frequency':
                `${toFixedFloat(curFreqKhz / 1e6, 3)}GHz / ${
                    toFixedFloat(maxFreqKhz / 1e6, 3)}GHz (${freqPercentage}%)`,
          };
        }),
      });
    }
  }

  updateCpuUsageData(physcialCpuUsage: Array<Array<CpuUsage|null>>) {
    const flattenCpuUsage: (CpuUsage)[] =
        physcialCpuUsage.flat().filter(usage => usage !== null);
    const averageUsage = getAverageCpuUsage(flattenCpuUsage);

    const usagePercentage =
        averageUsage.systemPercentage + averageUsage.userPercentage;
    this.$.infoCard.updateDisplayedInfo(1, {
      'Overall': `${toFixedFloat(usagePercentage, 2)}%`,
      'System': `${toFixedFloat(averageUsage.systemPercentage, 2)}%`,
      'User': `${toFixedFloat(averageUsage.userPercentage, 2)}%`,
      'Idle': `${toFixedFloat(averageUsage.idlePercentage, 2)}%`,
    });
  }

  updateExpanded(isExpanded: boolean) {
    this.$.infoCard.updateExpanded(isExpanded);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'healthd-internals-cpu-card': HealthdInternalsCpuCardElement;
  }
}

customElements.define(
    HealthdInternalsCpuCardElement.is, HealthdInternalsCpuCardElement);
