// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './info_card.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {HealthdApiMemoryResult, HealthdApiTelemetryResult, SystemZramInfo} from '../../utils/externs.js';
import {getFormattedMemory, getFormattedMemoryFromRaw, getFormattedMemoryWithPercentage, MemoryUnitEnum} from '../../utils/memory_utils.js';
import {toFixedFloat} from '../../utils/number_utils.js';

import type {HealthdInternalsInfoCardElement} from './info_card.js';
import {getTemplate} from './memory_card.html.js';

export interface HealthdInternalsMemoryCardElement {
  $: {
    infoCard: HealthdInternalsInfoCardElement,
    memoryUnitSelector: HTMLSelectElement,
  };
}

export class HealthdInternalsMemoryCardElement extends PolymerElement {
  static get is() {
    return 'healthd-internals-memory-card';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      memoryUnit: {type: String},
    };
  }

  override connectedCallback() {
    super.connectedCallback();

    this.$.infoCard.appendCardRow('INFO', true);
    this.$.infoCard.appendCardRow('SWAP', true);
    this.$.infoCard.appendCardRow('ZRAM', true);
    this.$.infoCard.appendCardRow('DETAILS');
  }

  // Displayed memory unit.
  private memoryUnit: MemoryUnitEnum = MemoryUnitEnum.AUTO;

  // The latest memory data to display.
  private latestMemoryInfo?: HealthdApiMemoryResult;
  private latestZramInfo?: SystemZramInfo;

  updateTelemetryData(data: HealthdApiTelemetryResult) {
    this.latestMemoryInfo = data.memory;
    this.refreshMemoryCard();
  }

  updateZramData(zram: SystemZramInfo) {
    this.latestZramInfo = zram;
    this.refreshMemoryCard();
  }

  updateExpanded(isExpanded: boolean) {
    this.$.infoCard.updateExpanded(isExpanded);
  }

  private refreshMemoryCard() {
    if (this.latestMemoryInfo !== undefined) {
      const memory = this.latestMemoryInfo;
      this.updateInfoRow(
          parseInt(memory.totalMemoryKib), parseInt(memory.freeMemoryKib),
          parseInt(memory.availableMemoryKib));
      if (memory.totalSwapMemoryKib !== undefined &&
          memory.freeSwapMemoryKib !== undefined) {
        this.updateSwapRow(
            parseInt(memory.totalSwapMemoryKib),
            parseInt(memory.freeSwapMemoryKib));
      }
      this.updateDetailsRow(memory);
    }
    if (this.latestZramInfo !== undefined) {
      this.updateZramRow(this.latestZramInfo);
    }
  }

  private updateInfoRow(
      totalMemoryKib: number, freeMemoryKib: number,
      availableMemoryKib: number) {
    const usedMemoryKib = totalMemoryKib - availableMemoryKib;
    const unit = this.memoryUnit;
    this.$.infoCard.updateDisplayedInfo(0, {
      'Total': getFormattedMemory(unit, totalMemoryKib),
      'Used':
          getFormattedMemoryWithPercentage(unit, usedMemoryKib, totalMemoryKib),
      'Avail': getFormattedMemoryWithPercentage(
          unit, availableMemoryKib, totalMemoryKib),
      'Free':
          getFormattedMemoryWithPercentage(unit, freeMemoryKib, totalMemoryKib),
    });
  }

  private updateSwapRow(totalSwapMemoryKib: number, freeSwapMemoryKib: number) {
    const usedSwapMemoryKib = totalSwapMemoryKib - freeSwapMemoryKib;
    const unit = this.memoryUnit;
    this.$.infoCard.updateDisplayedInfo(1, {
      'Total': getFormattedMemory(unit, totalSwapMemoryKib),
      'Used': getFormattedMemoryWithPercentage(
          unit, usedSwapMemoryKib, totalSwapMemoryKib),
      'Free': getFormattedMemoryWithPercentage(
          unit, freeSwapMemoryKib, totalSwapMemoryKib),
    });
  }

  private updateZramRow(zram: SystemZramInfo) {
    const totalUsedMemoryKib = parseInt(zram.totalUsedMemory) / 1024;
    const originalDataSizeKib = parseInt(zram.originalDataSize) / 1024;
    const compressedDataSizeKib = parseInt(zram.compressedDataSize) / 1024;

    // In general, higher compression ratio means better compression.
    const compressionRatio = originalDataSizeKib / compressedDataSizeKib;
    const spaceReductionPercentage =
        (originalDataSizeKib - compressedDataSizeKib) / originalDataSizeKib *
        100;

    const unit = this.memoryUnit;
    this.$.infoCard.updateDisplayedInfo(2, {
      'Total Used': getFormattedMemory(unit, totalUsedMemoryKib),
      'Original Size': getFormattedMemory(unit, originalDataSizeKib),
      'Compressed Size': getFormattedMemory(unit, compressedDataSizeKib),
      'Compression Ratio': toFixedFloat(compressionRatio, 2),
      'Space Reduction': `${toFixedFloat(spaceReductionPercentage, 2)}%`,
    });
  }

  private updateDetailsRow(memory: HealthdApiMemoryResult) {
    const unit = this.memoryUnit;
    this.$.infoCard.updateDisplayedInfo(3, {
      'Buffers': getFormattedMemoryFromRaw(unit, memory.buffersKib),
      'Page Cache': getFormattedMemoryFromRaw(unit, memory.pageCacheKib),
      'Shared': getFormattedMemoryFromRaw(unit, memory.sharedMemoryKib),
      'Active': getFormattedMemoryFromRaw(unit, memory.activeMemoryKib),
      'Inactive': getFormattedMemoryFromRaw(unit, memory.inactiveMemoryKib),
      'Total Slab': getFormattedMemoryFromRaw(unit, memory.totalSlabMemoryKib),
      'Reclaimable Slab':
          getFormattedMemoryFromRaw(unit, memory.reclaimableSlabMemoryKib),
      'Unreclaimable Slab':
          getFormattedMemoryFromRaw(unit, memory.unreclaimableSlabMemoryKib),
      'Cached Swap':
          getFormattedMemoryFromRaw(unit, memory.cachedSwapMemoryKib),
    });
  }



  private onMemoryUnitChanged() {
    this.memoryUnit = this.$.memoryUnitSelector.value as MemoryUnitEnum;
    this.refreshMemoryCard();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'healthd-internals-memory-card': HealthdInternalsMemoryCardElement;
  }
}

customElements.define(
    HealthdInternalsMemoryCardElement.is, HealthdInternalsMemoryCardElement);
