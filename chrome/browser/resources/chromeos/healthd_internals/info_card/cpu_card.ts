// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './info_card.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {HealthdApiPhysicalCpuResult, HealthdApiTelemetryResult} from '../externs.js';

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

    this.$.infoCard.appendCardRow('INFO');
  }

  // Whether the rows of physical CPUs are initialized.
  private isInitialized: boolean = false;

  updateTelemetryData(data: HealthdApiTelemetryResult) {
    this.$.infoCard.updateDisplayedInfo(0, {
      'architecture': data.cpu.architecture,
      'numTotalThreads': data.cpu.numTotalThreads,
    });

    const physicalCpus: HealthdApiPhysicalCpuResult[] = data.cpu.physicalCpus;
    if (!this.isInitialized) {
      for (let i: number = 0; i < physicalCpus.length; ++i) {
        this.$.infoCard.appendCardRow(`PHYSCICAL CPU #${i}`);
      }
      this.$.infoCard.refreshComponents();
      this.isInitialized = true;
    }

    const nextIdx: number = 1;
    for (let i: number = 0; i < physicalCpus.length; ++i) {
      this.$.infoCard.updateDisplayedInfo(nextIdx + i, {
        'modelName': physicalCpus[i].modelName,
        'logicalCpus': physicalCpus[i].logicalCpus.map((logicalCpu) => {
          return {
            'coreId': logicalCpu.coreId,
            'currentFrequency': logicalCpu.frequency.current,
            'maxFrequency': logicalCpu.frequency.max,
          };
        }),
      });
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'healthd-internals-cpu-card': HealthdInternalsCpuCardElement;
  }
}

customElements.define(
    HealthdInternalsCpuCardElement.is, HealthdInternalsCpuCardElement);
