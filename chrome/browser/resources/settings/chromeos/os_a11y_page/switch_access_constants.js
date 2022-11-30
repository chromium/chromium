// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Maps an action to its pref name.
 * @const {!Object<SwitchAccessCommand, string>}
 */
export const actionToPref = {
  select: 'settings.a11y.switch_access.select.device_key_codes',
  next: 'settings.a11y.switch_access.next.device_key_codes',
  previous: 'settings.a11y.switch_access.previous.device_key_codes',
};

/**
 * The pref name for whether auto-scan is enabled.
 * @const {string}
 */
export const AUTO_SCAN_ENABLED_PREF =
    'settings.a11y.switch_access.auto_scan.enabled';

/**
 * The pref name for the auto-scan speed.
 * @const {string}
 */
export const AUTO_SCAN_SPEED_PREF =
    'settings.a11y.switch_access.auto_scan.speed_ms';

/**
 * The pref name for the auto-scan speed specific to the onscreen keyboard.
 * @const {string}
 */
export const AUTO_SCAN_KEYBOARD_SPEED_PREF =
    'settings.a11y.switch_access.auto_scan.keyboard.speed_ms';

/**
 * The values that the auto-scan speed slider can have, in ms.
 * @const {!Array<number>}
 */
export const AUTO_SCAN_SPEED_RANGE_MS = [
  4000, 3900, 3800, 3700, 3600, 3500, 3400, 3300, 3200, 3100, 3000, 2900,
  2800, 2700, 2600, 2500, 2400, 2300, 2200, 2100, 2000, 1900, 1800, 1700,
  1600, 1500, 1400, 1300, 1200, 1100, 1000, 900,  800,  700,
];

/**
 * The default auto-scan value, in ms.
 * @const {number}
 */
export const DEFAULT_AUTO_SCAN_SPEED_MS = 1800;

/**
 * Contexts the assignment pane can be located in.
 * @enum {string}
 */
export const AssignmentContext = {
  DIALOG: 'dialog',
  SETUP_GUIDE: 'setupGuide',
};

/**
 * Available commands.
 * @enum {string}
 */
export const SwitchAccessCommand = {
  NEXT: 'next',
  PREVIOUS: 'previous',
  SELECT: 'select',
};

/**
 * Possible device types for Switch Access.
 * @enum {string}
 */
export const SwitchAccessDeviceType = {
  INTERNAL: 'internal',
  USB: 'usb',
  BLUETOOTH: 'bluetooth',
  UNKNOWN: 'unknown',
};
