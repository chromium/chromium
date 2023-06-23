// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines DeviceNameState and NameUpdateResult enum.
 */

/**
 * DeviceNameState stores information about the states of the device name.
 * Numerical values from this enum must stay in sync with the C++ enum in
 * device_name_store.h.
 */
export enum DeviceNameState {
  // We can modify the device name.
  CAN_BE_MODIFIED = 0,

  // We cannot modify the device name because of active policies.
  CANNOT_BE_MODIFIED_BECAUSE_OF_POLICIES = 1,

  // We cannot modify the device name because user is not device
  // owner.
  CANNOT_BE_MODIFIED_BECAUSE_NOT_DEVICE_OWNER = 2,
}

/**
 * NameUpdateResult stores information about the result of the name update
 * attempt. Numerical values from this enum must stay in sync with the C++ enum
 * in device_name_store.h.
 */
export enum SetDeviceNameResult {
  // Update was successful.
  UPDATE_SUCCESSFUL = 0,

  // Update was unsuccessful because it is prohibited by policy.
  ERROR_DUE_TO_POLICY = 1,

  // Update was unsuccessful because user is not the device owner.
  ERROR_DUE_TO_NOT_DEVICE_OWNER = 2,

  // Update was unsuccessful because user input an invalid name.
  ERROR_DUE_TO_INVALID_INPUT = 3,
}
