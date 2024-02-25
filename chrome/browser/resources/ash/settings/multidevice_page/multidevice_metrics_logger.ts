// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/js/assert.js';

/**
 * Location where Smart Lock was toggled on/off.
 */
export enum SmartLockToggleLocation {
  MULTIDEVICE_PAGE = 0,
  LOCK_SCREEN_SETTINGS = 1,
}

/**
 * This enum is tied directly to a UMA enum defined in
 * //tools/metrics/histograms/enums.xml, and should always reflect it (do not
 * change one without changing the other).
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 */
enum SmartLockToggle {
  ENABLED_ON_MULTIDEVICE_PAGE = 0,
  DISABLED_ON_MULTIDEVICE_PAGE = 1,
  ENABLED_ON_LOCK_SCREEN_SETTINGS = 2,
  DISABLED_ON_LOCK_SCREEN_SETTINGS = 3,
  MAX = 4,
}

const SMART_LOCK_TOGGLE_HISTOGRAM_NAME = 'SmartLock.Toggle';

/**
 * Records a metric for when Smart Lock is enabled/disabled in Settings
 * indicating which toggle was used and whether Smart Lock was enabled or
 * disabled.
 */
export function recordSmartLockToggleMetric(
    smartLockToggleLocation: SmartLockToggleLocation, enabled: boolean): void {
  chrome.send('metricsHandler:recordInHistogram', [
    SMART_LOCK_TOGGLE_HISTOGRAM_NAME,
    getSmartLockToggleValue(smartLockToggleLocation, enabled),
    SmartLockToggle.MAX,
  ]);
}

/**
 * Look up the correct SmartLock.Toggle historgram value to emit when Smart
 * Lock is enabled/disabled in the given location in Settings.
 */
export function getSmartLockToggleValue(
    smartLockToggleLocation: SmartLockToggleLocation,
    enabled: boolean): SmartLockToggle {
  switch (smartLockToggleLocation) {
    case SmartLockToggleLocation.MULTIDEVICE_PAGE:
      return enabled ? SmartLockToggle.ENABLED_ON_MULTIDEVICE_PAGE :
                       SmartLockToggle.DISABLED_ON_MULTIDEVICE_PAGE;
    case SmartLockToggleLocation.LOCK_SCREEN_SETTINGS:
      return enabled ? SmartLockToggle.ENABLED_ON_LOCK_SCREEN_SETTINGS :
                       SmartLockToggle.DISABLED_ON_LOCK_SCREEN_SETTINGS;
    default:
      assertNotReached('Invalid smartLockToggleLocation');
  }
}
