// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
// clang-format on

/**
 * @fileoverview A helper object used by the "Google Play Store" (ARC) section
 * to retrieve information about android apps.
 */

/**
 * Type definition of AndroidAppsInfo entry. |playStoreEnabled| indicates that
 * Play Store is enabled. |settingsAppAvailable| indicates that Android settings
 * app is registered in the system.
 * @typedef {{
 *   playStoreEnabled: boolean,
 *   settingsAppAvailable: boolean,
 * }}
 * @see chrome/browser/ui/webui/settings/chromeos/android_apps_handler.cc
 */
/* #export */ let AndroidAppsInfo;

cr.define('settings', function() {
  /** @interface */
  /* #export */ class AndroidAppsBrowserProxy {
    requestAndroidAppsInfo() {}

    /**
     * @param {boolean} keyboardAction True if the app was opened using a
     *     keyboard action.
     */
    showAndroidAppsSettings(keyboardAction) {}
  }

  /**
   * @implements {settings.AndroidAppsBrowserProxy}
   */
  /* #export */ class AndroidAppsBrowserProxyImpl {
    /** @override */
    requestAndroidAppsInfo() {
      chrome.send('requestAndroidAppsInfo');
    }

    /** @override */
    showAndroidAppsSettings(keyboardAction) {
      chrome.send('showAndroidAppsSettings', [keyboardAction]);
    }
  }

  // The singleton instance_ can be replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(AndroidAppsBrowserProxyImpl);

  // #cr_define_end
  return {
    AndroidAppsBrowserProxy: AndroidAppsBrowserProxy,
    AndroidAppsBrowserProxyImpl: AndroidAppsBrowserProxyImpl,
  };
});
