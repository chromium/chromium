// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export enum ButtonState {
  ENABLED = 1,
  DISABLED = 2,
  HIDDEN = 3,
}

export enum ButtonName {
  CANCEL = 1,
  PAIR = 2,
}

export interface ButtonBarState {
  cancel: ButtonState;
  pair: ButtonState;
}

/**
 * Device pairing authentication type. During device pairing, a device might
 * require additional authentication before pairing can be completed. This
 * is used to define which type of authentication is required.
 */
export enum PairingAuthType {
  NONE = 1,
  REQUEST_PIN_CODE = 2,
  REQUEST_PASSKEY = 3,
  DISPLAY_PIN_CODE = 4,
  DISPLAY_PASSKEY = 5,
  CONFIRM_PASSKEY = 6,
  AUTHORIZE_PAIRING = 7,
}

export enum DeviceItemState {
  DEFAULT = 1,
  PAIRING = 2,
  FAILED = 3,
}

export enum BatteryType {
  DEFAULT = 1,
  LEFT_BUD = 2,
  CASE = 3,
  RIGHT_BUD = 4,
}
