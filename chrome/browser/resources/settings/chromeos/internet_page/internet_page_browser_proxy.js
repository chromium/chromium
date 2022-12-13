// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview A helper object used for Internet page. */

import {addWebUIListener} from 'chrome://resources/ash/common/cr.m.js';

/** @interface */
export class InternetPageBrowserProxy {
  /**
   * Shows the account details page of a cellular network.
   * @param {string} guid
   */
  showCarrierAccountDetail(guid) {}

  /**
   * Shows the Cellular activation UI.
   * @param {string} guid
   */
  showCellularSetupUI(guid) {}

  /**
   * Shows the Portal Signin.
   * @param {string} guid
   */
  showPortalSignin(guid) {}

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

/** @type {?InternetPageBrowserProxy} */
let instance = null;

/**
 * @implements {InternetPageBrowserProxy}
 */
export class InternetPageBrowserProxyImpl {
  /** @return {!InternetPageBrowserProxy} */
  static getInstance() {
    return instance || (instance = new InternetPageBrowserProxyImpl());
  }

  /** @param {!InternetPageBrowserProxy} obj */
  static setInstance(obj) {
    instance = obj;
  }

  /** @override */
  showCarrierAccountDetail(guid) {
    chrome.send('showCarrierAccountDetail', [guid]);
  }

  /** @override */
  showCellularSetupUI(guid) {
    chrome.send('showCellularSetupUI', [guid]);
  }

  /** @override */
  showPortalSignin(guid) {
    chrome.send('showPortalSignin', [guid]);
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
    addWebUIListener('sendGmsCoreNotificationsDisabledDeviceNames', callback);
  }
}
