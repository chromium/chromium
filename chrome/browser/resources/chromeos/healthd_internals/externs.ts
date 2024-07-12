// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview External interfaces used by chrome://healthd-internals.
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
  battery: HealthdApiBatteryResult;
  thermals: HealthdApiThermalResult[];
}
