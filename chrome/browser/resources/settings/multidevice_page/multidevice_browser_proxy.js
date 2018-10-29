// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('settings');

/**
 * An object containing messages for web permissisions origin
 * and the messages multidevice feature state.
 *
 * @typedef {{origin: string,
 *            enabled: boolean}}
 */
settings.AndroidSmsInfo;

cr.define('settings', function() {
  /** @interface */
  class MultiDeviceBrowserProxy {
    showMultiDeviceSetupDialog() {}

    /** @return {!Promise<!MultiDevicePageContentData>} */
    getPageContentData() {}

    /**
     * @param {!settings.MultiDeviceFeature} feature The feature whose state
     *     should be set.
     * @param {boolean} enabled Whether the feature should be turned off or on.
     * @param {string=} opt_authToken Proof that the user is authenticated.
     *     Needed to enable Smart Lock, and Better Together Suite if the Smart
     *     Lock user pref is enabled.
     * @return {!Promise<boolean>} Whether the operation was successful.
     */
    setFeatureEnabledState(feature, enabled, opt_authToken) {}

    removeHostDevice() {}

    retryPendingHostSetup() {}

    /**
     * Called when the "Set Up" button is clicked to open the Android Messages
     * PWA.
     */
    setUpAndroidSms() {}

    /**
     * Returns the value of the preference controlling whether Smart Lock may be
     * used to sign-in the user (as opposed to unlocking the screen).
     * @return {!Promise<boolean>}
     */
    getSmartLockSignInEnabled() {}

    /**
     * Sets the value of the preference controlling whether Smart Lock may be
     * used to sign-in the user (as opposed to unlocking the screen).
     * @param {boolean} enabled
     * @param {string=} opt_authToken Authentication token used to restrict
     *    edit access to the Smart Lock sign-in pref.
     */
    setSmartLockSignInEnabled(enabled, opt_authToken) {}

    /**
     * Returns the value of the preference controlling whether Smart Lock
     * sign-in is allowed.
     * @return {!Promise<boolean>}
     */
    getSmartLockSignInAllowed() {}

    /**
     * Returns android messages info with messages feature state
     * and messages for web permissions origin.
     * @return {!Promise<!settings.AndroidSmsInfo>} Android SMS Info
     */
    getAndroidSmsInfo() {}
  }

  /**
   * @implements {settings.MultiDeviceBrowserProxy}
   */
  class MultiDeviceBrowserProxyImpl {
    /** @override */
    showMultiDeviceSetupDialog() {
      chrome.send('showMultiDeviceSetupDialog');
    }

    /** @override */
    getPageContentData() {
      return cr.sendWithPromise('getPageContentData');
    }

    /** @override */
    setFeatureEnabledState(feature, enabled, opt_authToken) {
      return cr.sendWithPromise(
          'setFeatureEnabledState', feature, enabled, opt_authToken);
    }

    /** @override */
    removeHostDevice() {
      chrome.send('removeHostDevice');
    }

    /** @override */
    retryPendingHostSetup() {
      chrome.send('retryPendingHostSetup');
    }

    /** @override */
    setUpAndroidSms() {
      chrome.send('setUpAndroidSms');
    }

    /** @override */
    getSmartLockSignInEnabled() {
      return cr.sendWithPromise('getSmartLockSignInEnabled');
    }

    /** @override */
    setSmartLockSignInEnabled(enabled, opt_authToken) {
      chrome.send('setSmartLockSignInEnabled', [enabled, opt_authToken]);
    }

    /** @override */
    getSmartLockSignInAllowed() {
      return cr.sendWithPromise('getSmartLockSignInAllowed');
    }

    /** @override */
    getAndroidSmsInfo() {
      return cr.sendWithPromise('getAndroidSmsInfo');
    }
  }

  cr.addSingletonGetter(MultiDeviceBrowserProxyImpl);

  return {
    MultiDeviceBrowserProxy: MultiDeviceBrowserProxy,
    MultiDeviceBrowserProxyImpl: MultiDeviceBrowserProxyImpl,
  };
});
