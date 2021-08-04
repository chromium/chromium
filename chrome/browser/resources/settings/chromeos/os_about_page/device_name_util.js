// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines DeviceNameState enum.
 */

/**
 * DeviceNameState stores information about the states of the device name.
 * Numerical values from this enum must stay in sync with the C++ enum in
 * device_name_store.h.
 * @enum {number}
 */
export const DeviceNameState = {
  // We can modify the device name.
  CAN_BE_MODIFIED: 0,

  // We cannot modify the device name because of active policies.
  CANNOT_BE_MODIFIED_BECAUSE_OF_POLICIES: 1,

  // We cannot modify the device name because user is not device
  // owner.
  CANNOT_BE_MODIFIED_BECAUSE_NOT_DEVICE_OWNER: 2,
};