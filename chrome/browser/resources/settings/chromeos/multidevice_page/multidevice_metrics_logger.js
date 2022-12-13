// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/ash/common/assert.js';

/**
 * Location where Smart Lock was toggled on/off.
 * @enum {number}
 */
export const SmartLockToggleLocation = {
  MULTIDEVICE_PAGE: 0,
  LOCK_SCREEN_SETTINGS: 1,
};

/**
 * This enum is tied directly to a UMA enum defined in
 * //tools/metrics/histograms/enums.xml, and should always reflect it (do not
 * change one without changing the other).
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 * @enum {number}
 */
const SmartLockToggle = {
  ENABLED_ON_MULTIDEVICE_PAGE: 0,
  DISABLED_ON_MULTIDEVICE_PAGE: 1,
  ENABLED_ON_LOCK_SCREEN_SETTINGS: 2,
  DISABLED_ON_LOCK_SCREEN_SETTINGS: 3,
  MAX: 4,
};

const SmartLockToggleHistogramName = 'SmartLock.Toggle';

/**
 * Records a metric for when Smart Lock is enabled/disabled in Settings
 * indicating which toggle was used and whether Smart Lock was enabled or
 * disabled.
 * @param {SmartLockToggleLocation} smartLockToggleLocation
 * @param {boolean} enabled
 */
export function recordSmartLockToggleMetric(smartLockToggleLocation, enabled) {
  chrome.send('metricsHandler:recordInHistogram', [
    SmartLockToggleHistogramName,
    getSmartLockToggleValue_(smartLockToggleLocation, enabled),
    SmartLockToggle.MAX,
  ]);
}

/**
 * Look up the correct SmartLock.Toggle historgram value to emit when Smart
 * Lock is enabled/disabled in the given location in Settings.
 * @param {SmartLockToggleLocation} smartLockToggleLocation
 * @param {boolean} enabled
 * @return {SmartLockToggle}
 * @private
 */
function getSmartLockToggleValue_(smartLockToggleLocation, enabled) {
  switch (smartLockToggleLocation) {
    case SmartLockToggleLocation.MULTIDEVICE_PAGE:
      return enabled ? SmartLockToggle.ENABLED_ON_MULTIDEVICE_PAGE :
                       SmartLockToggle.DISABLED_ON_MULTIDEVICE_PAGE;
    case SmartLockToggleLocation.LOCK_SCREEN_SETTINGS:
      return enabled ? SmartLockToggle.ENABLED_ON_LOCK_SCREEN_SETTINGS :
                       SmartLockToggle.DISABLED_ON_LOCK_SCREEN_SETTINGS;
  }

  assertNotReached('Invalid smartLockToggleLocation');
  return SmartLockToggle.DISABLED_ON_MULTIDEVICE_PAGE;
}
