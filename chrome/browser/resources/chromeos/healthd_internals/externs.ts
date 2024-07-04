// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview External interfaces used by chrome://healthd-internals.
 */

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
  thermals: HealthdApiThermalResult[];
}
