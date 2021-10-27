// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Type aliases for the mojo API.
 *
 * TODO(michaelcheco): When the fake API is replaced by mojo these can be
 * re-aliased to the corresponding mojo types, or replaced by them.
 */

/**
 * The priority of an update.
 * @enum {number}
 */
export const UpdatePriority = {
  kLow: 0,
  kMedium: 1,
  kHigh: 2,
  kCritical: 3,
};

/**
 * @typedef {{
 *   deviceId: string,
 *   deviceName: string,
 *   version: string,
 *   description: string,
 *   priority: !UpdatePriority,
 *   updateModeInstructions: ?string,
 *   screenshotUrl: ?string,
 * }}
 */
export let FirmwareUpdate;

/**
 * Type alias for UpdateObserver.
 * @typedef {{
 *   onUpdateListChanged: !function(!Array<!FirmwareUpdate>)
 * }}
 */
export let UpdateObserver;

/**
 * Type of UpdateProviderInterface.ObservePeripheralUpdatesFunction function.
 * @typedef {!function(!UpdateObserver): void}
 */
export let ObservePeripheralUpdatesFunction;

/**
 * Type alias for the UpdateProviderInterface.
 * TODO(michaelcheco): Replace with a real mojo type when implemented.
 * @typedef {{
 *   observePeripheralUpdates: !ObservePeripheralUpdatesFunction,
 * }}
 */
export let UpdateProviderInterface;
