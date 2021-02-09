// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';

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
   * @param {string} content
   * @return {Promise<!Array>}
   */
  importONC(content) {}

  /**
   * @return {Promise<!Array>}
   */
  openCellularActivationUi() {}

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
  importONC(content) {
    return sendWithPromise('importONC', content);
  }

  /** @override */
  openCellularActivationUi() {
    return sendWithPromise('openCellularActivationUi');
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
}

addSingletonGetter(NetworkUIBrowserProxyImpl);
