// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {CloudPrintInterface} from './cloud_print_interface.js';
import {CloudPrintInterfaceJS} from './cloud_print_interface_js.js';
import {CloudPrintInterfaceNative} from './cloud_print_interface_native.js';
import {NativeLayer} from './native_layer.js';

/** @type {?CloudPrintInterface} */
let instance = null;

/**
 * @param {string} baseUrl Base part of the Google Cloud Print service URL
 *     with no trailing slash. For example,
 *     'https://www.google.com/cloudprint'.
 * @param {!NativeLayer} nativeLayer Native layer instance.
 * @param {boolean} isInAppKioskMode Whether the print preview is in App
 *     Kiosk mode.
 * @param {string} uiLocale The UI locale, for example "en-US" or "fr".
 * @return {!CloudPrintInterface}
 */
export function getCloudPrintInterface(
    baseUrl, nativeLayer, isInAppKioskMode, uiLocale) {
  if (instance === null) {
    if (loadTimeData.getBoolean('cloudPrinterHandlerEnabled')) {
      instance = new CloudPrintInterfaceNative();
    } else {
      instance = new CloudPrintInterfaceJS(
          baseUrl, nativeLayer, isInAppKioskMode, uiLocale);
    }
  }
  return instance;
}

/**
 * Sets the CloudPrintInterface singleton instance, useful for testing.
 * @param {!CloudPrintInterface} cloudPrintInterface
 */
export function setCloudPrintInterfaceForTesting(cloudPrintInterface) {
  instance = cloudPrintInterface;
}
