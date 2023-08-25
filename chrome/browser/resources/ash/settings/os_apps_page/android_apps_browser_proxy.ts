// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the "Google Play Store" (ARC) section
 * to retrieve information about android apps.
 */

/**
 * Type definition of AndroidAppsInfo entry. |playStoreEnabled| indicates that
 * Play Store is enabled. |settingsAppAvailable| indicates that Android settings
 * app is registered in the system.
 * @see chrome/browser/ui/webui/settings/ash/android_apps_handler.cc
 */
export interface AndroidAppsInfo {
  playStoreEnabled: boolean;
  settingsAppAvailable: boolean;
}

export interface AndroidAppsBrowserProxy {
  requestAndroidAppsInfo(): void;

  /**
   * @param keyboardAction True if the app was opened using a keyboard action.
   */
  showAndroidAppsSettings(keyboardAction: boolean): void;

  openGooglePlayStore(url: string): void;
}

let instance: AndroidAppsBrowserProxy|null = null;

export class AndroidAppsBrowserProxyImpl implements AndroidAppsBrowserProxy {
  static getInstance(): AndroidAppsBrowserProxy {
    return instance || (instance = new AndroidAppsBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: AndroidAppsBrowserProxy): void {
    instance = obj;
  }

  requestAndroidAppsInfo(): void {
    chrome.send('requestAndroidAppsInfo');
  }

  showAndroidAppsSettings(keyboardAction: boolean): void {
    chrome.send('showAndroidAppsSettings', [keyboardAction]);
  }

  openGooglePlayStore(url: string): void {
    chrome.send('showPlayStoreApps', [url]);
  }
}
