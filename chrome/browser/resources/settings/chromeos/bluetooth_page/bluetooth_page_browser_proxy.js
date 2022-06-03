// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';

/** @interface */
export class BluetoothPageBrowserProxy {
  /** @return {!Promise<!boolean>} */
  isDeviceBlockedByPolicy(address) {}
}

/**
 * @implements {BluetoothPageBrowserProxy}
 */
export class BluetoothPageBrowserProxyImpl {
  /** @override */
  isDeviceBlockedByPolicy(address) {
    return sendWithPromise('isDeviceBlockedByPolicy', address);
  }
}

addSingletonGetter(BluetoothPageBrowserProxyImpl);
