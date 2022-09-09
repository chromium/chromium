// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.m.js';

/** @interface */
export class BluetoothPageBrowserProxy {
  /** @return {!Promise<!boolean>} */
  isDeviceBlockedByPolicy(address) {}
}

/** @type {?BluetoothPageBrowserProxy} */
let instance = null;

/**
 * @implements {BluetoothPageBrowserProxy}
 */
export class BluetoothPageBrowserProxyImpl {
  /** @return {!BluetoothPageBrowserProxy} */
  static getInstance() {
    return instance || (instance = new BluetoothPageBrowserProxyImpl());
  }

  /** @param {!BluetoothPageBrowserProxy} obj */
  static setInstanceForTesting(obj) {
    instance = obj;
  }

  /** @override */
  isDeviceBlockedByPolicy(address) {
    return sendWithPromise('isDeviceBlockedByPolicy', address);
  }
}
