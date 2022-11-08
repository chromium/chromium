// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/ash/common/cr_deprecated.js';

/** @interface */
export class CfmNetworkSettingsBrowserProxy {
  /** @param {string} guid */
  showNetworkDetails(guid) {}

  /** @param {string} guid */
  showNetworkConfig(guid) {}

  showAddWifi() {}

  showManageCerts() {}
}

/** @implements {CfmNetworkSettingsBrowserProxy} */
export class CfmNetworkSettingsBrowserProxyImpl {
  /** @override */
  showNetworkDetails(guid) {
    chrome.send('showNetworkDetails', [guid]);
  }

  /** @override */
  showNetworkConfig(guid) {
    chrome.send('showNetworkConfig', [guid]);
  }

  /** @override */
  showAddWifi() {
    chrome.send('showAddWifi');
  }

  /** @override */
  showManageCerts() {
    chrome.send('showManageCerts');
  }
}

addSingletonGetter(CfmNetworkSettingsBrowserProxyImpl);
