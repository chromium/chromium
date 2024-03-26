// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrosBluetoothConfig, CrosBluetoothConfigInterface} from '//resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';

/**
 * @fileoverview
 * Wrapper for CrosBluetoothConfig that provides the ability to inject a fake
 * CrosBluetoothConfig implementation for tests.
 */

let bluetoothConfig: CrosBluetoothConfigInterface|undefined;

export function setBluetoothConfigForTesting(
    testBluetoothConfig?: CrosBluetoothConfigInterface): void {
  bluetoothConfig = testBluetoothConfig;
}

export function getBluetoothConfig(): CrosBluetoothConfigInterface {
  if (bluetoothConfig) {
    return bluetoothConfig;
  }

  bluetoothConfig = CrosBluetoothConfig.getRemote();
  return bluetoothConfig;
}
