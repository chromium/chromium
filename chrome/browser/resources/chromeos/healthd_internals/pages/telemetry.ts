// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cr_button/cr_button.js';
import '../info_card/cpu_card.js';
import '../info_card/fan_card.js';
import '../info_card/memory_card.js';
import '../info_card/power_card.js';
import '../info_card/thermal_card.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {CpuUsage} from '../cpu_usage_helper.js';
import {HealthdApiTelemetryResult, SystemZramInfo} from '../externs.js';
import type {HealthdInternalsCpuCardElement} from '../info_card/cpu_card.js';
import type {HealthdInternalsFanCardElement} from '../info_card/fan_card.js';
import type {HealthdInternalsMemoryCardElement} from '../info_card/memory_card.js';
import type {HealthdInternalsPowerCardElement} from '../info_card/power_card.js';
import type {HealthdInternalsThermalCardElement} from '../info_card/thermal_card.js';

import {getTemplate} from './telemetry.html.js';
import {HealthdInternalsPage} from './utils/page_interface.js';
import {UiUpdateHelper} from './utils/ui_update_helper.js';

export interface HealthdInternalsTelemetryElement {
  $: {
    cpuCard: HealthdInternalsCpuCardElement,
    fanCard: HealthdInternalsFanCardElement,
    memoryCard: HealthdInternalsMemoryCardElement,
    powerCard: HealthdInternalsPowerCardElement,
    thermalCard: HealthdInternalsThermalCardElement,
  };
}

export class HealthdInternalsTelemetryElement extends PolymerElement implements
    HealthdInternalsPage {
  static get is() {
    return 'healthd-internals-telemetry';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      lastUpdateTime: {type: String},
    };
  }


  override connectedCallback() {
    super.connectedCallback();

    this.updateHelper = new UiUpdateHelper(() => {
      this.refreshTelemetryPage();
    });
  }

  // Latest raw data from healthd.
  private healthdData?: HealthdApiTelemetryResult = undefined;

  // Other latest data.
  private cpuUsageData?: (CpuUsage|null)[][] = undefined;
  private zramData?: SystemZramInfo = undefined;

  // Helper for updating UI regularly. Init in `connectedCallback`.
  private updateHelper: UiUpdateHelper;

  // The time that the telemetry data is last updated.
  private lastUpdateTime: string = '';

  updateTelemetryData(data: HealthdApiTelemetryResult) {
    const isInitilized: boolean = this.healthdData !== undefined;
    this.healthdData = data;
    if (!isInitilized) {
      // Display data as soon as we first receive it.
      this.refreshTelemetryPage();
    }
  }

  updateCpuUsageData(physcialCpuUsage: (CpuUsage|null)[][]) {
    const isInitilized: boolean = this.cpuUsageData !== undefined;
    this.cpuUsageData = physcialCpuUsage;
    if (!isInitilized) {
      // Display data as soon as we first receive it.
      this.refreshTelemetryPage();
    }
  }

  updateZramData(zram: SystemZramInfo) {
    const isInitilized: boolean = this.zramData !== undefined;
    this.zramData = zram;
    if (!isInitilized) {
      // Display data as soon as we first receive it.
      this.refreshTelemetryPage();
    }
  }

  updateVisibility(isVisible: boolean) {
    this.updateHelper.updateVisibility(isVisible);
  }

  updateUiUpdateInterval(intervalSeconds: number) {
    this.updateHelper.updateUiUpdateInterval(intervalSeconds);
  }

  private refreshTelemetryPage() {
    if (this.healthdData !== undefined) {
      this.$.cpuCard.updateTelemetryData(this.healthdData);
      this.$.fanCard.updateTelemetryData(this.healthdData);
      this.$.memoryCard.updateTelemetryData(this.healthdData);
      this.$.powerCard.updateTelemetryData(this.healthdData);
      this.$.thermalCard.updateTelemetryData(this.healthdData);
    }
    if (this.cpuUsageData !== undefined) {
      this.$.cpuCard.updateCpuUsageData(this.cpuUsageData);
    }
    if (this.zramData !== undefined) {
      this.$.memoryCard.updateZramData(this.zramData);
    }
    this.lastUpdateTime = new Date().toLocaleTimeString();
  }

  private onExpandAllButtonClick() {
    this.updateCardsExpanded(true);
  }

  private onCollapseAllButtonClicked() {
    this.updateCardsExpanded(false);
  }

  private updateCardsExpanded(isExpanded: boolean) {
    this.$.cpuCard.updateExpanded(isExpanded);
    this.$.fanCard.updateExpanded(isExpanded);
    this.$.memoryCard.updateExpanded(isExpanded);
    this.$.powerCard.updateExpanded(isExpanded);
    this.$.thermalCard.updateExpanded(isExpanded);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'healthd-internals-telemetry': HealthdInternalsTelemetryElement;
  }
}

customElements.define(
    HealthdInternalsTelemetryElement.is, HealthdInternalsTelemetryElement);
