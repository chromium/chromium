// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
// clang-format on

/**
 * Type definition of AndroidAppsInfo entry. |playStoreEnabled| indicates that
 * Play Store is enabled. |settingsAppAvailable| indicates that Android settings
 * app is registered in the system.
 * @see chrome/browser/ui/webui/settings/chromeos/android_apps_handler.cc
 */
export interface AndroidAppsInfo {
  playStoreEnabled: boolean;
  settingsAppAvailable: boolean;
}

/**
 * An object containing messages for web permissisions origin
 * and the messages multidevice feature state.
 */
export interface AndroidSmsInfo {
  origin: string;
  enabled: boolean;
}

export interface AndroidInfoBrowserProxy {
  /**
   * Returns android messages info with messages feature state
   * and messages for web permissions origin.
   */
  getAndroidSmsInfo(): Promise<AndroidSmsInfo>;

  requestAndroidAppsInfo(): void;
}

export class AndroidInfoBrowserProxyImpl implements AndroidInfoBrowserProxy {
  getAndroidSmsInfo() {
    return sendWithPromise('getAndroidSmsInfo');
  }

  requestAndroidAppsInfo() {
    chrome.send('requestAndroidAppsInfo');
  }

  static getInstance(): AndroidInfoBrowserProxy {
    return instance || (instance = new AndroidInfoBrowserProxyImpl());
  }

  static setInstance(obj: AndroidInfoBrowserProxy) {
    instance = obj;
  }
}

let instance: AndroidInfoBrowserProxy|null = null;
