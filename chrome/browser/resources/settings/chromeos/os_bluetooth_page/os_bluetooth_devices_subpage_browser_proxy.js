// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @interface */
export class OsBluetoothDevicesSubpageBrowserProxy {
  /**
   * Requests whether the Fast Pair feature is supported by the device.
   * Returned by the 'fast-pair-device-supported' WebUI listener event.
   */
  requestFastPairDeviceSupport() {}

  /**
   * Requests Fast Pair Saved Devices opt-in status and list of devices.
   * Returned by the 'fast-pair-saved-devices-opt-in-status' and the
   * 'fast-pair-saved-devices-list' WebUI listener event.
   */
  requestFastPairSavedDevices() {}

  /**
   * Invokes the removal of a Fast Pair device by the account key |accountKey|
   * from a user's account.
   * @param {string} accountKey
   */
  deleteFastPairSavedDevice(accountKey) {}
}

/** @type {?OsBluetoothDevicesSubpageBrowserProxy} */
let instance = null;

/**
 * @implements {OsBluetoothDevicesSubpageBrowserProxy}
 */
export class OsBluetoothDevicesSubpageBrowserProxyImpl {
  /** @return {!OsBluetoothDevicesSubpageBrowserProxy} */
  static getInstance() {
    return instance ||
        (instance = new OsBluetoothDevicesSubpageBrowserProxyImpl());
  }

  /** @param {!OsBluetoothDevicesSubpageBrowserProxy} obj */
  static setInstanceForTesting(obj) {
    instance = obj;
  }

  /** @override */
  requestFastPairDeviceSupport() {
    chrome.send('requestFastPairDeviceSupportStatus');
  }

  /** @override */
  requestFastPairSavedDevices() {
    chrome.send('loadSavedDevicePage');
  }

  /**
   * @override
   * @param {string} accountKey
   */
  deleteFastPairSavedDevice(accountKey) {
    chrome.send('removeSavedDevice', [accountKey]);
  }
}
