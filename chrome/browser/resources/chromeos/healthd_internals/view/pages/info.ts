// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {CpuUsage} from '../../model/cpu_usage_helper.js';
import {getAverageCpuUsage} from '../../utils/cpu_usage_utils.js';
import type {HealthdApiTelemetryResult, SystemZramInfo} from '../../utils/externs.js';
import {getFormattedMemory, MemoryUnitEnum} from '../../utils/memory_utils.js';
import {toFixedFloat} from '../../utils/number_utils.js';
import type {HealthdInternalsPage} from '../../utils/page_interface.js';
import {UiUpdateHelper} from '../../utils/ui_update_helper.js';

import {getTemplate} from './info.html.js';

export class HealthdInternalsInfoElement extends PolymerElement implements
    HealthdInternalsPage {
  static get is() {
    return 'healthd-internals-info';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      infoNumOfCpu: {type: String},
      infoCpuUsage: {type: String},
      infoCpuKernel: {type: String},
      infoCpuUser: {type: String},
      infoCpuIdle: {type: String},
      infoMemoryTotal: {type: String},
      infoMemoryUsed: {type: String},
      infoMemorySwapTotal: {type: String},
      infoMemorySwapUsed: {type: String},
      infoZramTotal: {type: String},
      infoZramOrig: {type: String},
      infoZramCompr: {type: String},
      infoZramComprRatio: {type: String},
    };
  }


  override connectedCallback() {
    super.connectedCallback();

    this.updateHelper = new UiUpdateHelper(() => {
      this.refreshInfoPage();
    });
  }

  // Latest raw data from healthd.
  private healthdData?: HealthdApiTelemetryResult = undefined;

  // Other latest data.
  private cpuUsageData?: Array<Array<CpuUsage|null>> = undefined;
  private zramData?: SystemZramInfo = undefined;

  // Helper for updating UI regularly. Init in `connectedCallback`.
  private updateHelper: UiUpdateHelper;

  // Displayed info.
  private infoNumOfCpu: string = '0';
  private infoCpuUsage: string = '0.00%';
  private infoCpuKernel: string = '0.00%';
  private infoCpuUser: string = '0.00%';
  private infoCpuIdle: string = '0.00%';
  private infoMemoryTotal: string = '0';
  private infoMemoryUsed: string = '0';
  private infoMemorySwapTotal: string = '0';
  private infoMemorySwapUsed: string = '0';
  private infoZramTotal: string = '0';
  private infoZramOrig: string = '0';
  private infoZramCompr: string = '0';
  private infoZramComprRatio: string = 'N/A';
  private infoZramSpaceRedu: string = '0.00%';

  updateTelemetryData(data: HealthdApiTelemetryResult) {
    const isInitilized: boolean = this.healthdData !== undefined;
    this.healthdData = data;
    if (!isInitilized) {
      // Display data as soon as we first receive it.
      this.renderHealthdData();
    }
  }

  updateCpuUsageData(physcialCpuUsage: Array<Array<CpuUsage|null>>) {
    const isInitilized: boolean = this.cpuUsageData !== undefined;
    this.cpuUsageData = physcialCpuUsage;
    if (!isInitilized) {
      // Display data as soon as we first receive it.
      this.renderCpuUsageData();
    }
  }

  updateZramData(zram: SystemZramInfo) {
    const isInitilized: boolean = this.zramData !== undefined;
    this.zramData = zram;
    if (!isInitilized) {
      // Display data as soon as we first receive it.
      this.renderZramData();
    }
  }

  updateVisibility(isVisible: boolean) {
    this.updateHelper.updateVisibility(isVisible);
  }

  updateUiUpdateInterval(intervalSeconds: number) {
    this.updateHelper.updateUiUpdateInterval(intervalSeconds);
  }

  private refreshInfoPage() {
    this.renderHealthdData();
    this.renderCpuUsageData();
    this.renderZramData();
  }

  private renderHealthdData() {
    if (this.healthdData === undefined) {
      return;
    }

    this.infoNumOfCpu = this.healthdData.cpu.numTotalThreads;

    const memory = this.healthdData.memory;
    const unit = MemoryUnitEnum.AUTO;

    const totalMemoryKib = parseInt(memory.totalMemoryKib);
    const usedMemoryKib = totalMemoryKib - parseInt(memory.availableMemoryKib);
    this.infoMemoryTotal = getFormattedMemory(unit, totalMemoryKib);
    this.infoMemoryUsed = getFormattedMemory(unit, usedMemoryKib);

    if (memory.totalSwapMemoryKib !== undefined &&
        memory.freeSwapMemoryKib !== undefined) {
      const totalSwapMemoryKib = parseInt(memory.totalSwapMemoryKib);
      const usedSwapMemoryKib =
          totalSwapMemoryKib - parseInt(memory.freeSwapMemoryKib);
      this.infoMemorySwapTotal = getFormattedMemory(unit, totalSwapMemoryKib);
      this.infoMemorySwapUsed = getFormattedMemory(unit, usedSwapMemoryKib);
    }
  }

  private renderCpuUsageData() {
    if (this.cpuUsageData === undefined) {
      return;
    }

    const flattenCpuUsage: (CpuUsage)[] =
        this.cpuUsageData.flat().filter(usage => usage !== null);
    const averageUsage = getAverageCpuUsage(flattenCpuUsage);
    const usagePercentage =
        averageUsage.systemPercentage + averageUsage.userPercentage;

    this.infoCpuUsage = `${toFixedFloat(usagePercentage, 2)}%`;
    this.infoCpuKernel = `${toFixedFloat(averageUsage.systemPercentage, 2)}%`;
    this.infoCpuUser = `${toFixedFloat(averageUsage.userPercentage, 2)}%`;
    this.infoCpuIdle = `${toFixedFloat(averageUsage.idlePercentage, 2)}%`;
  }

  private renderZramData() {
    if (this.zramData === undefined) {
      return;
    }

    const totalUsedMemoryKib = parseInt(this.zramData.totalUsedMemory) / 1024;
    const originalDataSizeKib = parseInt(this.zramData.originalDataSize) / 1024;
    const compressedDataSizeKib =
        parseInt(this.zramData.compressedDataSize) / 1024;

    // In general, higher compression ratio means better compression.
    const compressionRatio = originalDataSizeKib / compressedDataSizeKib;
    const spaceReductionPercentage =
        (originalDataSizeKib - compressedDataSizeKib) / originalDataSizeKib *
        100;

    const unit = MemoryUnitEnum.AUTO;
    this.infoZramTotal = getFormattedMemory(unit, totalUsedMemoryKib);
    this.infoZramOrig = getFormattedMemory(unit, originalDataSizeKib);
    this.infoZramCompr = getFormattedMemory(unit, compressedDataSizeKib);
    this.infoZramComprRatio = toFixedFloat(compressionRatio, 2);
    this.infoZramSpaceRedu = `${toFixedFloat(spaceReductionPercentage, 2)}%`;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'healthd-internals-info': HealthdInternalsInfoElement;
  }
}

customElements.define(
    HealthdInternalsInfoElement.is, HealthdInternalsInfoElement);
