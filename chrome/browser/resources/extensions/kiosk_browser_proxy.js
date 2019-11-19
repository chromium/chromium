// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "Kiosk" dialog to interact with
 * the browser.
 */

import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';

/**
 * @typedef {{
 *   kioskEnabled: boolean,
 *   autoLaunchEnabled: boolean
 * }}
 */
let KioskSettings;

/**
 * @typedef {{
 *   id: string,
 *   name: string,
 *   iconURL: string,
 *   autoLaunch: boolean,
 *   isLoading: boolean
 * }}
 */
export let KioskApp;

/**
 * @typedef {{
 *   apps: !Array<!KioskApp>,
 *   disableBailout: boolean,
 *   hasAutoLaunchApp: boolean
 * }}
 */
export let KioskAppSettings;

/** @interface */
export class KioskBrowserProxy {
  /** @param {string} appId */
  addKioskApp(appId) {}

  /** @param {string} appId */
  disableKioskAutoLaunch(appId) {}

  /** @param {string} appId */
  enableKioskAutoLaunch(appId) {}

  /** @return {!Promise<!KioskAppSettings>} */
  getKioskAppSettings() {}

  /** @return {!Promise<!KioskSettings>} */
  initializeKioskAppSettings() {}

  /** @param {string} appId */
  removeKioskApp(appId) {}

  /** @param {boolean} disableBailout */
  setDisableBailoutShortcut(disableBailout) {}
}

/** @implements {KioskBrowserProxy} */
export class KioskBrowserProxyImpl {
  /** @override */
  initializeKioskAppSettings() {
    return sendWithPromise('initializeKioskAppSettings');
  }

  /** @override */
  getKioskAppSettings() {
    return sendWithPromise('getKioskAppSettings');
  }

  /** @override */
  addKioskApp(appId) {
    chrome.send('addKioskApp', [appId]);
  }

  /** @override */
  disableKioskAutoLaunch(appId) {
    chrome.send('disableKioskAutoLaunch', [appId]);
  }

  /** @override */
  enableKioskAutoLaunch(appId) {
    chrome.send('enableKioskAutoLaunch', [appId]);
  }

  /** @override */
  removeKioskApp(appId) {
    chrome.send('removeKioskApp', [appId]);
  }

  /** @override */
  setDisableBailoutShortcut(disableBailout) {
    chrome.send('setDisableBailoutShortcut', [disableBailout]);
  }
}

addSingletonGetter(KioskBrowserProxyImpl);
