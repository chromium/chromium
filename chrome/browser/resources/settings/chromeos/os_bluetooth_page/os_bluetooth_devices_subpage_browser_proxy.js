// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
// clang-format on

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
}

// The singleton instance_ is replaced with a test version of this wrapper
// during testing.
addSingletonGetter(OsBluetoothDevicesSubpageBrowserProxyImpl);