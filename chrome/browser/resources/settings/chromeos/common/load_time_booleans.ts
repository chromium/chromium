// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Getters for loadTimeData booleans used throughout CrOS Settings.
 * Export them as functions so they reload the values when overridden in tests.
 */
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

export function isGuest(): boolean {
  return loadTimeData.getBoolean('isGuest');
}

export function isAccountManagerEnabled(): boolean {
  return loadTimeData.getBoolean('isAccountManagerEnabled');
}

export function isCrostiniAllowed(): boolean {
  return loadTimeData.getBoolean('isCrostiniAllowed');
}

export function isCrostiniSupported(): boolean {
  return loadTimeData.getBoolean('isCrostiniSupported');
}

export function isKerberosEnabled(): boolean {
  return loadTimeData.getBoolean('isKerberosEnabled');
}

export function isPowerwashAllowed(): boolean {
  return loadTimeData.getBoolean('allowPowerwash');
}
