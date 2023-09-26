// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview A helper object used for Internet page. */

import {addWebUiListener} from 'chrome://resources/js/cr.js';

export interface InternetPageBrowserProxy {
  /**
   * Shows the account details page of a cellular network.
   */
  showCarrierAccountDetail(guid: string): void;

  /**
   * Shows the Cellular activation UI.
   */
  showCellularSetupUi(guid: string): void;

  /**
   * Shows the Portal Signin.
   */
  showPortalSignin(guid: string): void;

  /**
   * Shows configuration for external VPNs. Includes ThirdParty (extension
   * configured) VPNs, and Arc VPNs.
   */
  configureThirdPartyVpn(guid: string): void;

  /**
   * Sends an add VPN request to the external VPN provider (ThirdParty VPN
   * extension or Arc VPN provider app).
   */
  addThirdPartyVpn(appId: string): void;

  /**
   * Requests that Chrome send the list of devices whose "Google Play
   * Services" notifications are disabled (these notifications must be enabled
   * to utilize Instant Tethering). The names will be provided via
   * setGmsCoreNotificationsDisabledDeviceNamesCallback().
   */
  requestGmsCoreNotificationsDisabledDeviceNames(): void;

  /**
   * Sets the callback to be used to receive the list of devices whose "Google
   * Play Services" notifications are disabled. |callback| is invoked with an
   * array of the names of these devices; note that if no devices have this
   * property, the provided list of device names is empty.
   */
  setGmsCoreNotificationsDisabledDeviceNamesCallback(
      callback: (deviceNames: string[]) => void): void;
}

let instance: InternetPageBrowserProxy|null = null;

export class InternetPageBrowserProxyImpl implements InternetPageBrowserProxy {
  static getInstance(): InternetPageBrowserProxy {
    return instance || (instance = new InternetPageBrowserProxyImpl());
  }

  static setInstance(obj: InternetPageBrowserProxy): void {
    instance = obj;
  }

  showCarrierAccountDetail(guid: string): void {
    chrome.send('showCarrierAccountDetail', [guid]);
  }

  showCellularSetupUi(guid: string): void {
    chrome.send('showCellularSetupUi', [guid]);
  }

  showPortalSignin(guid: string): void {
    chrome.send('showPortalSignin', [guid]);
  }

  configureThirdPartyVpn(guid: string): void {
    chrome.send('configureThirdPartyVpn', [guid]);
  }

  addThirdPartyVpn(appId: string): void {
    chrome.send('addThirdPartyVpn', [appId]);
  }

  requestGmsCoreNotificationsDisabledDeviceNames(): void {
    chrome.send('requestGmsCoreNotificationsDisabledDeviceNames');
  }

  setGmsCoreNotificationsDisabledDeviceNamesCallback(
      callback: (deviceNames: string[]) => void): void {
    addWebUiListener('sendGmsCoreNotificationsDisabledDeviceNames', callback);
  }
}
