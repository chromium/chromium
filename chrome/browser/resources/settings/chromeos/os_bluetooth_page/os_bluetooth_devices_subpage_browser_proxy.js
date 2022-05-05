// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @interface */
export class OsBluetoothDevicesSubpageBrowserProxy {
  /**
   * Requests whether the Fast Pair feature is supported by the device.
   * Returned by the 'fast-pair-device-supported' WebUI listener event.
   */
  requestFastPairDeviceSupport() {}
}

/**
 * @implements {OsBluetoothDevicesSubpageBrowserProxy}
 */
export class OsBluetoothDevicesSubpageBrowserProxyImpl {
  /** @override */
  requestFastPairDeviceSupport() {
    chrome.send('requestFastPairDeviceSupportStatus');
  }

  /** @return {!OsBluetoothDevicesSubpageBrowserProxy} */
  static getInstance() {
    return instance ||
        (instance = new OsBluetoothDevicesSubpageBrowserProxyImpl());
  }

  /** @param {!OsBluetoothDevicesSubpageBrowserProxy} obj */
  static setInstance(obj) {
    instance = obj;
  }
}

/** @type {?OsBluetoothDevicesSubpageBrowserProxy} */
let instance = null;
