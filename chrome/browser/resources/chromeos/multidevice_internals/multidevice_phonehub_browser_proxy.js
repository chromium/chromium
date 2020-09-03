// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
import {BrowserTabsModel, FeatureStatus, PhoneStatusModel} from './types.js';

/**
 * JavaScript hooks into the native WebUI handler for Phonehub tab.
 */
export class MultidevicePhoneHubBrowserProxy {
  /**
   * Enables or disables the FakePhoneHubManager.
   * @param {boolean} enabled Whether to enable the fake phone hub manager.
   */
  setFakePhoneHubManagerEnabled(enabled) {
    chrome.send('setFakePhoneHubManagerEnabled', [enabled]);
  }

  /**
   * Enables or disables the FakePhoneHubManager.
   * @param {!FeatureStatus} featureStatus The status of the feature.
   */
  setFeatureStatus(featureStatus) {
    chrome.send('setFeatureStatus', [featureStatus]);
  }

  /**
   * Sets the phone model.
   * @param {!PhoneStatusModel} phoneStatusModel The phone status with fake
   *     values.
   */
  setFakePhoneStatus(phoneStatusModel) {
    chrome.send('setFakePhoneStatus', [phoneStatusModel]);
  }

  /**
   * Sets the browser tabs model.
   * @param {!BrowserTabsModel} browserTabsModel The browser tab model with fake
   *     values.
   */
  setBrowserTabs(browserTabsModel) {
    chrome.send('setBrowserTabs', [browserTabsModel]);
  }
}

addSingletonGetter(MultidevicePhoneHubBrowserProxy);
