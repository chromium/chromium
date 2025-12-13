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

export function isChild(): boolean {
  return loadTimeData.getBoolean('isChild');
}

export function isSecondaryUser(): boolean {
  return loadTimeData.getBoolean('isSecondaryUser');
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

export function isAppParentalControlsFeatureAvailable(): boolean {
  return loadTimeData.getBoolean('isAppParentalControlsFeatureAvailable');
}

// Crostini page
export function isCrostiniAllowed(): boolean {
  return loadTimeData.getBoolean('isCrostiniAllowed');
}

export function isCrostiniSupported(): boolean {
  return loadTimeData.getBoolean('isCrostiniSupported');
}

// Device page
export function isExternalStorageEnabled(): boolean {
  return loadTimeData.getBoolean('isExternalStorageEnabled');
}

export function isDisplayBrightnessControlInSettingsEnabled(): boolean {
  return loadTimeData.getBoolean('enableDisplayBrightnessControlInSettings');
}

export function isSkyVaultEnabled(): boolean {
  return loadTimeData.getBoolean('enableSkyVault');
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

// Reset page
export function isSanitizeAllowed(): boolean {
  return loadTimeData.getBoolean('allowSanitize');
}

// Search page
export function isQuickAnswersSupported(): boolean {
  return loadTimeData.getBoolean('isQuickAnswersSupported');
}

export function isMagicBoostFeatureEnabled(): boolean {
  return loadTimeData.getBoolean('isMagicBoostFeatureEnabled');
}

export function isMagicBoostNoticeBannerVisible(): boolean {
  return loadTimeData.getBoolean('isMagicBoostNoticeBannerVisible');
}

export function isLobsterSettingsToggleVisible(): boolean {
  return loadTimeData.getBoolean('isLobsterSettingsToggleVisible');
}

export function isScannerSettingsToggleVisible(): boolean {
  return loadTimeData.getBoolean('isScannerSettingsToggleVisible');
}

// System preferences page
export function shouldShowStartup(): boolean {
  return loadTimeData.getBoolean('shouldShowStartup');
}

// Power page
export function isBatteryChargeLimitAvailable(): boolean {
  return loadTimeData.getBoolean('isBatteryChargeLimitAvailable');
}
