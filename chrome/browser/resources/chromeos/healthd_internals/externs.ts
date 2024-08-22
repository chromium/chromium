// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview External interfaces used by chrome://healthd-internals.
 *
 * See chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom
 * for more details.
 */

/**
 * `getHealthdTelemetryInfo` battery result.
 */
export interface HealthdApiBatteryResult {
  currentNow: number;
  voltageNow: number;
  chargeNow: number;
}

/**
 * `getHealthdTelemetryInfo` battery result.
 */
export interface HealthdApiCpuResult {
  architecture: string;
  numTotalThreads: string;
  physicalCpus: HealthdApiPhysicalCpuResult[];
  temperatureChannels: HealthdApiTemperatureChannelResult[];
}

export interface HealthdApiPhysicalCpuResult {
  modelName?: string;
  logicalCpus: HealthdApiLogicalCpuResult[];
}

export interface HealthdApiLogicalCpuResult {
  coreId: string;
  frequency: HealthdApiCpuScalingFrequencyKhz;
  executionTime: HealthdApiCpuExecutionTimeUserHz;
}

export interface HealthdApiCpuScalingFrequencyKhz {
  current: string;
  max: string;
}

export interface HealthdApiCpuExecutionTimeUserHz {
  user: string;
  system: string;
  idle: string;
}

export interface HealthdApiTemperatureChannelResult {
  label?: string;
  temperatureCelsius: number;
}

/**
 * `getHealthdTelemetryInfo` fan result.
 */
export interface HealthdApiFanResult {
  speedRpm: string;
}

/**
 * `getHealthdTelemetryInfo` memory result.
 */
export interface HealthdApiMemoryResult {
  availableMemoryKib: string;
  freeMemoryKib: string;
  totalMemoryKib: string;
  buffersKib?: string;
  pageCacheKib?: string;
  sharedMemoryKib?: string;
  activeMemoryKib?: string;
  inactiveMemoryKib?: string;
  totalSwapMemoryKib?: string;
  freeSwapMemoryKib?: string;
  cachedSwapMemoryKib?: string;
  totalSlabMemoryKib?: string;
  reclaimableSlabMemoryKib?: string;
  unreclaimableSlabMemoryKib?: string;
}

/**
 * `getHealthdTelemetryInfo` thermal result.
 */
export interface HealthdApiThermalResult {
  name: string;
  source: string;
  temperatureCelsius: number;
}

/**
 * `getHealthdTelemetryInfo` api result.
 */
export interface HealthdApiTelemetryResult {
  battery?: HealthdApiBatteryResult;
  cpu: HealthdApiCpuResult;
  fans: HealthdApiFanResult[];
  memory: HealthdApiMemoryResult;
  thermals: HealthdApiThermalResult[];
}

/**
 * `getHealthdProcessInfo` api result.
 */
export interface HealthdApiProcessResult {
  processes: HealthdApiProcessInfo[];
}

/**
 * Detailed process info. See `ProcessInfo` in
 * chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom
 * for more details.
 */
export interface HealthdApiProcessInfo {
  command: string;
  userId: string;
  priority: number;
  nice: number;
  uptimeTicks: string;
  state: string;
  residentMemoryKib: string;
  readSystemCallsCount: string;
  writeSystemCallsCount: string;
  name?: string;
  parentProcessId: string;
  processGroupId: string;
  threadsNumber: string;
  processId: string;
}

/**
 * `getHealthdInternalsFeatureFlag` api result.
 */
export interface HealthdInternalsFeatureFlagResult {
  tabsDisplayed: boolean;
}
