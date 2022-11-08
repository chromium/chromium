// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "Kiosk" dialog to interact with
 * the browser.
 */

import {sendWithPromise} from 'chrome://resources/js/cr.js';

export interface KioskSettings {
  kioskEnabled: boolean;
  autoLaunchEnabled: boolean;
}

export interface KioskApp {
  id: string;
  name: string;
  iconURL: string;
  autoLaunch: boolean;
  isLoading: boolean;
}

export interface KioskAppSettings {
  apps: KioskApp[];
  disableBailout: boolean;
  hasAutoLaunchApp: boolean;
}

/** @interface */
export interface KioskBrowserProxy {
  addKioskApp(appId: string): void;
  disableKioskAutoLaunch(appId: string): void;
  enableKioskAutoLaunch(appId: string): void;
  getKioskAppSettings(): Promise<KioskAppSettings>;
  initializeKioskAppSettings(): Promise<KioskSettings>;
  removeKioskApp(appId: string): void;
  setDisableBailoutShortcut(disableBailout: boolean): void;
}

export class KioskBrowserProxyImpl implements KioskBrowserProxy {
  initializeKioskAppSettings() {
    return sendWithPromise('initializeKioskAppSettings');
  }

  getKioskAppSettings() {
    return sendWithPromise('getKioskAppSettings');
  }

  addKioskApp(appId: string) {
    chrome.send('addKioskApp', [appId]);
  }

  disableKioskAutoLaunch(appId: string) {
    chrome.send('disableKioskAutoLaunch', [appId]);
  }

  enableKioskAutoLaunch(appId: string) {
    chrome.send('enableKioskAutoLaunch', [appId]);
  }

  removeKioskApp(appId: string) {
    chrome.send('removeKioskApp', [appId]);
  }

  setDisableBailoutShortcut(disableBailout: boolean) {
    chrome.send('setDisableBailoutShortcut', [disableBailout]);
  }

  static getInstance(): KioskBrowserProxy {
    return instance || (instance = new KioskBrowserProxyImpl());
  }

  static setInstance(obj: KioskBrowserProxy) {
    instance = obj;
  }
}

let instance: KioskBrowserProxy|null = null;
