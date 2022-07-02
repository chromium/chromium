// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {DeviceNameState, SetDeviceNameResult} from './device_name_util.js';

/**
 * @typedef {{
 *   deviceName: string,
 *   deviceNameState: !DeviceNameState,
 * }}
 */
export let DeviceNameMetadata;

/** @interface */
export class DeviceNameBrowserProxy {
  /**
   * Notifies the system that the page is ready for the device name.
   * @return {!Promise<!DeviceNameMetadata>}
   */
  notifyReadyForDeviceName() {}

  /**
   * Attempts to set the device name to the new name entered by the user.
   * @param {string} name
   * @return {!Promise<!SetDeviceNameResult>}
   */
  attemptSetDeviceName(name) {}
}

/** @type {?DeviceNameBrowserProxy} */
let instance = null;

/**
 * @implements {DeviceNameBrowserProxy}
 */
export class DeviceNameBrowserProxyImpl {
  /** @return {!DeviceNameBrowserProxy} */
  static getInstance() {
    return instance || (instance = new DeviceNameBrowserProxyImpl());
  }

  /** @param {!DeviceNameBrowserProxy} obj */
  static setInstanceForTesting(obj) {
    instance = obj;
  }

  /** @override */
  notifyReadyForDeviceName() {
    return chrome.send('notifyReadyForDeviceName');
  }

  /** @override */
  attemptSetDeviceName(name) {
    return sendWithPromise('attemptSetDeviceName', name);
  }
}
