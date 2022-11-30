// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @enum {number} */
export const ButtonState = {
  ENABLED: 1,
  DISABLED: 2,
  HIDDEN: 3,
};

/** @enum {number} */
export const ButtonName = {
  CANCEL: 1,
  PAIR: 2,
};

/**
 * @typedef {{
 *   cancel: (!ButtonState),
 *   pair: (!ButtonState),
 * }}
 */
export let ButtonBarState;

/**
 * Device pairing authentication type. During device pairing, a device might
 * require additional authentication before pairing can be completed. This
 * is used to define which type of authentication is required.
 * @enum {number}
 */
export const PairingAuthType = {
  NONE: 1,
  REQUEST_PIN_CODE: 2,
  REQUEST_PASSKEY: 3,
  DISPLAY_PIN_CODE: 4,
  DISPLAY_PASSKEY: 5,
  CONFIRM_PASSKEY: 6,
  AUTHORIZE_PAIRING: 7,
};

/** @enum {number} */
export const DeviceItemState = {
  DEFAULT: 1,
  PAIRING: 2,
  FAILED: 3,
};

/** @enum {number} */
export const BatteryType = {
  DEFAULT: 1,
  LEFT_BUD: 2,
  CASE: 3,
  RIGHT_BUD: 4,
};
