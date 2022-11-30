// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrosBluetoothConfig, CrosBluetoothConfigInterface} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';

/**
 * @fileoverview
 * Wrapper for CrosBluetoothConfig that provides the ability to inject a fake
 * CrosBluetoothConfig implementation for tests.
 */

/** @type {?CrosBluetoothConfigInterface} */
let bluetoothConfig = null;

/**
 * @param {?CrosBluetoothConfigInterface}
 *     testBluetoothConfig The CrosBluetoothConfig implementation used for
 *                         testing. Passing null reverses the override.
 */
export function setBluetoothConfigForTesting(testBluetoothConfig) {
  bluetoothConfig = testBluetoothConfig;
}

/**
 * @return {!CrosBluetoothConfigInterface}
 */
export function getBluetoothConfig() {
  if (bluetoothConfig) {
    return bluetoothConfig;
  }

  bluetoothConfig = CrosBluetoothConfig.getRemote();
  return bluetoothConfig;
}
