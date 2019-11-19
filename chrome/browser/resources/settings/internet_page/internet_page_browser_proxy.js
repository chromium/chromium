// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview A helper object used for Internet page. */
cr.exportPath('settings');

cr.define('settings', function() {
  /** @interface */
  class InternetPageBrowserProxy {
    /**
     * Shows the Cellular activation UI.
     * @param {string} guid
     */
    showCellularSetupUI(guid) {}

    /**
     * Shows configuration for external VPNs. Includes ThirdParty (extension
     * configured) VPNs, and Arc VPNs.
     * @param {string} guid
     */
    configureThirdPartyVpn(guid) {}

    /**
     * Sends an add VPN request to the external VPN provider (ThirdParty VPN
     * extension or Arc VPN provider app).
     * @param {string} appId
     */
    addThirdPartyVpn(appId) {}

    /**
     * Requests that Chrome send the list of devices whose "Google Play
     * Services" notifications are disabled (these notifications must be enabled
     * to utilize Instant Tethering). The names will be provided via
     * setGmsCoreNotificationsDisabledDeviceNamesCallback().
     */
    requestGmsCoreNotificationsDisabledDeviceNames() {}

    /**
     * Sets the callback to be used to receive the list of devices whose "Google
     * Play Services" notifications are disabled. |callback| is invoked with an
     * array of the names of these devices; note that if no devices have this
     * property, the provided list of device names is empty.
     * @param {function(!Array<string>):void} callback
     */
    setGmsCoreNotificationsDisabledDeviceNamesCallback(callback) {}
  }

  /**
   * @implements {settings.InternetPageBrowserProxy}
   */
  class InternetPageBrowserProxyImpl {
    /** @override */
    showCellularSetupUI(guid) {
      chrome.send('showCellularSetupUI', [guid]);
    }

    /** @override */
    configureThirdPartyVpn(guid) {
      chrome.send('configureThirdPartyVpn', [guid]);
    }

    /** @override */
    addThirdPartyVpn(appId) {
      chrome.send('addThirdPartyVpn', [appId]);
    }

    /** @override */
    requestGmsCoreNotificationsDisabledDeviceNames() {
      chrome.send('requestGmsCoreNotificationsDisabledDeviceNames');
    }

    /** @override */
    setGmsCoreNotificationsDisabledDeviceNamesCallback(callback) {
      cr.addWebUIListener(
          'sendGmsCoreNotificationsDisabledDeviceNames', callback);
    }
  }

  cr.addSingletonGetter(InternetPageBrowserProxyImpl);

  return {
    InternetPageBrowserProxy: InternetPageBrowserProxy,
    InternetPageBrowserProxyImpl: InternetPageBrowserProxyImpl,
  };
});
