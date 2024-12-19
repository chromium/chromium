// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {toFixedFloat} from './number_utils.js';


/**
 * The value of memory unit in selected menu.
 */
export enum MemoryUnitEnum {
  AUTO = 'auto',
  GIBI = 'gibibyte',
  MEBI = 'mebibyte',
  KIBI = 'kibibyte',
}

/**
 * Get the formatted string for the memory value in string.
 *
 * @param memoryUnit - The memory unit for the displayed string.
 * @param rawMemoryKiB - The raw memory string.
 * @returns - The formatted memory string.
 */
export function getFormattedMemoryFromRaw(
    memoryUnit: MemoryUnitEnum, rawMemoryKiB?: string): string {
  if (rawMemoryKiB === undefined) {
    return 'N/A';
  }
  return getFormattedMemory(memoryUnit, parseInt(rawMemoryKiB));
}

/**
 * Get the formatted string with percentage for memory value.
 *
 * @param memoryUnit - The memory unit for the displayed string.
 * @param memory - The number of memory to be formatted.
 * @param totalMemory - Number of total memory, used to calculate percentage.
 * @returns - The formatted memory string.
 */
export function getFormattedMemoryWithPercentage(
    memoryUnit: MemoryUnitEnum, memory: number, totalMemory: number): string {
  return `${getFormattedMemory(memoryUnit, memory)} (${
      toFixedFloat(memory / totalMemory * 100, 2)}%)`;
}

/**
 * Get the formatted string for memory value.
 *
 * @param memoryUnit - The memory unit for the displayed string.
 * @param memory - The number of memory to be formatted.
 * @returns - The formatted memory string.
 */
export function getFormattedMemory(
    memoryUnit: MemoryUnitEnum, memory: number): string {
  switch (memoryUnit) {
    case MemoryUnitEnum.AUTO: {
      const units = ['KB', 'MB', 'GB'];
      let unitIdx = 0;
      while (memory > 1024 && unitIdx + 1 < units.length) {
        memory /= 1024;
        unitIdx++;
      }
      return `${toFixedFloat(memory, 2)} ${units[unitIdx]}`;
    }
    case MemoryUnitEnum.GIBI: {
      return `${toFixedFloat(memory / 1024 / 1024, 2)} GB`;
    }
    case MemoryUnitEnum.MEBI: {
      return `${toFixedFloat(memory / 1024, 2)} MB`;
    }
    case MemoryUnitEnum.KIBI: {
      return `${memory} KB`;
    }
    default: {
      console.error('Unknown memory unit: ', memoryUnit);
      return 'N/A';
    }
  }
}
