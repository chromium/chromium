// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {BluetoothDeviceProperties} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';

export class BluetoothDeviceBatteryInfoElement extends PolymerElement {
  device: BluetoothDeviceProperties;
}

declare global {
  interface HTMLElementTagNameMap {
    'bluetooth-device-battery-info': BluetoothDeviceBatteryInfoElement;
  }
}
