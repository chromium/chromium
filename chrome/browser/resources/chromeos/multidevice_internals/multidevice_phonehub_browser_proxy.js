// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

import {BrowserTabsModel, FeatureStatus, FindMyDeviceStatus, Notification, PhoneStatusModel, TetherStatus} from './types.js';

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
   * Causes the onboarding flow to show if enabled.
   * @param {boolean} shouldShowOnboardingFlow Whether to show onboarding flow.
   */
  setShowOnboardingFlow(shouldShowOnboardingFlow) {
    chrome.send('setShowOnboardingFlow', [shouldShowOnboardingFlow]);
  }

  /**
   * Sets the phone name.
   * @param {string} phoneName
   */
  setFakePhoneName(phoneName) {
    chrome.send('setFakePhoneName', [phoneName]);
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

  /**
   * Sets a notification.
   * @param {!Notification} notification
   */
  setNotification(notification) {
    chrome.send('setNotification', [notification]);
  }

  /**
   * Removes a notification with the id |notificationId|
   * @param {number} notificationId
   */
  removeNotification(notificationId) {
    chrome.send('removeNotification', [notificationId]);
  }

  /**
   * Enables phone do not disturb.
   * @param {boolean} enabled
   */
  enableDnd(enabled) {
    chrome.send('enableDnd', [enabled]);
  }

  /**
   * Enables phone ringing.
   * @param {!FindMyDeviceStatus} findMyDeviceStatus
   */
  setFindMyDeviceStatus(findMyDeviceStatus) {
    chrome.send('setFindMyDeviceStatus', [findMyDeviceStatus]);
  }

  /**
   * Sets tether status.
   * @param {!TetherStatus} tetherStatus
   */
  setTetherStatus(tetherStatus) {
    chrome.send('setTetherStatus', [tetherStatus]);
  }
}

addSingletonGetter(MultidevicePhoneHubBrowserProxy);
