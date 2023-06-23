// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Getters for loadTimeData booleans used throughout CrOS Settings.
 * Export them as functions so they reload the values when overridden in tests.
 * Organize the getter functions by their respective pages.
 */
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

// General
export function isGuest(): boolean {
  return loadTimeData.getBoolean('isGuest');
}

export function isRevampWayfindingEnabled(): boolean {
  return loadTimeData.getBoolean('isRevampWayfindingEnabled');
}

// Apps page
export function androidAppsVisible(): boolean {
  return loadTimeData.getBoolean('androidAppsVisible');
}

export function isArcVmEnabled(): boolean {
  return loadTimeData.getBoolean('isArcVmEnabled');
}

export function isPlayStoreAvailable(): boolean {
  return loadTimeData.getBoolean('isPlayStoreAvailable');
}

export function isPluginVmAvailable(): boolean {
  return loadTimeData.getBoolean('isPluginVmAvailable');
}

// Crostini page
export function isCrostiniAllowed(): boolean {
  return loadTimeData.getBoolean('isCrostiniAllowed');
}

export function isCrostiniSupported(): boolean {
  return loadTimeData.getBoolean('isCrostiniSupported');
}

// Kerberos page
export function isKerberosEnabled(): boolean {
  return loadTimeData.getBoolean('isKerberosEnabled');
}

// People page
export function isAccountManagerEnabled(): boolean {
  return loadTimeData.getBoolean('isAccountManagerEnabled');
}

// Reset page
export function isPowerwashAllowed(): boolean {
  return loadTimeData.getBoolean('allowPowerwash');
}
