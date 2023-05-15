// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/ash/common/cr.m.js';
import {addSingletonGetter} from 'chrome://resources/ash/common/cr_deprecated.js';

/** @interface */
export class NetworkUIBrowserProxy {
  /** @param {string} type */
  addNetwork(type) {}

  /**
   * @param {string} type
   * @return {Promise<!Array>}
   */
  getShillDeviceProperties(type) {}

  /**
   * @return {Promise<!Array>}
   */
  getShillEthernetEAP() {}

  /**
   * @param {string} guid
   * @return {Promise<!Array>}
   */
  getShillNetworkProperties(guid) {}

  /**
   * @return {Promise<!Array>}
   */
  getFirstWifiNetworkProperties() {}

  /**
   * @param {string} content
   * @return {Promise<!Array>}
   */
  importONC(content) {}

  /**
   * @return {Promise<!Array>}
   */
  openCellularActivationUi() {}

  resetESimCache() {}

  /**
   * @param {string} debugging
   * @return {Promise<!Array>}
   */
  setShillDebugging(debugging) {}

  showAddNewWifi() {}

  /** @param {string} guid */
  showNetworkConfig(guid) {}

  /** @param {string} guid */
  showNetworkDetails(guid) {}

  /**
   * @param {Object<string>} options
   * @return {Promise<!Array>}
   */
  storeLogs(options) {}

  /**
   * @return {Promise<string>}
   */
  getHostname() {}

  /**
   * @param {string} hostname
   */
  setHostname(hostname) {}

  disableActiveESimProfile() {}

  resetEuicc() {}

  resetApnMigrator() {}

  /**
   * @return {Promise<string>}
   */
  getTetheringCapabilities() {}

  /**
   * @return {Promise<string>}
   */
  getTetheringStatus() {}

  /**
   * @return {Promise<string>}
   */
  getTetheringConfig() {}

  /**
   * @param {string} config
   * @return {Promise<string>}
   */
  setTetheringConfig(config) {}

  /**
   * @return {Promise<string>}
   */
  checkTetheringReadiness() {}

  /**
   * @param {boolean} enabled
   * @return {Promise<string>}
   */
  setTetheringEnabled(enabled) {}
}

/**
 * @implements {NetworkUIBrowserProxy}
 */
export class NetworkUIBrowserProxyImpl {
  /** @override */
  addNetwork(type) {
    chrome.send('addNetwork', [type]);
  }

  /** @override */
  getShillDeviceProperties(type) {
    return sendWithPromise('getShillDeviceProperties', type);
  }

  /** @override */
  getShillEthernetEAP() {
    return sendWithPromise('getShillEthernetEAP');
  }

  /** @override */
  getShillNetworkProperties(guid) {
    return sendWithPromise('getShillNetworkProperties', guid);
  }

  /** @override */
  getFirstWifiNetworkProperties() {
    return sendWithPromise('getFirstWifiNetworkProperties');
  }

  /** @override */
  importONC(content) {
    return sendWithPromise('importONC', content);
  }

  /** @override */
  openCellularActivationUi() {
    return sendWithPromise('openCellularActivationUi');
  }

  /** @override */
  resetESimCache() {
    chrome.send('resetESimCache');
  }

  /** @override */
  setShillDebugging(debugging) {
    return sendWithPromise('setShillDebugging', debugging);
  }

  /** @override */
  showAddNewWifi() {
    chrome.send('showAddNewWifi');
  }

  /** @override */
  showNetworkConfig(guid) {
    chrome.send('showNetworkConfig', [guid]);
  }

  /** @override */
  showNetworkDetails(guid) {
    chrome.send('showNetworkDetails', [guid]);
  }

  /** @override */
  storeLogs(options) {
    return sendWithPromise('storeLogs', options);
  }

  /**
   * @return {Promise<string>}
   */
  getHostname() {
    return sendWithPromise('getHostname');
  }

  /**
   * @param {string} hostname
   */
  setHostname(hostname) {
    chrome.send('setHostname', [hostname]);
  }

  /** @override */
  disableActiveESimProfile() {
    chrome.send('disableActiveESimProfile');
  }

  /** @override */
  resetEuicc() {
    chrome.send('resetEuicc');
  }

  /** @override */
  resetApnMigrator() {
    chrome.send('resetApnMigrator');
  }

  /**
   * @return {Promise<string>}
   */
  getTetheringCapabilities() {
    return sendWithPromise('getTetheringCapabilities');
  }

  /**
   * @return {Promise<string>}
   */
  getTetheringStatus() {
    return sendWithPromise('getTetheringStatus');
  }

  /**
   * @return {Promise<string>}
   */
  getTetheringConfig() {
    return sendWithPromise('getTetheringConfig');
  }

  /**
   * @param {string} config
   * @return {Promise<string>}
   */
  setTetheringConfig(config) {
    return sendWithPromise('setTetheringConfig', config);
  }

  /**
   * @return {Promise<string>}
   */
  checkTetheringReadiness() {
    return sendWithPromise('checkTetheringReadiness');
  }

  /**
   * @param {boolean} enabled
   * @return {Promise<string>}
   */
  setTetheringEnabled(enabled) {
    return sendWithPromise('setTetheringEnabled', enabled);
  }
}

addSingletonGetter(NetworkUIBrowserProxyImpl);
