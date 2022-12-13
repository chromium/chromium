// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/ash/common/cr.m.js';

import {MultiDeviceFeature, MultiDevicePageContentData, PhoneHubPermissionsSetupAction, PhoneHubPermissionsSetupFeatureCombination, PhoneHubPermissionsSetupFlowScreens} from './multidevice_constants.js';

/**
 * An object containing messages for web permissisions origin
 * and the messages multidevice feature state.
 *
 * @typedef {{origin: string,
 *            enabled: boolean}}
 */
export let AndroidSmsInfo;

/** @interface */
export class MultiDeviceBrowserProxy {
  showMultiDeviceSetupDialog() {}

  /** @return {!Promise<!MultiDevicePageContentData>} */
  getPageContentData() {}

  /**
   * @param {!MultiDeviceFeature} feature The feature whose state
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
   * @return {!Promise<!AndroidSmsInfo>} Android SMS Info
   */
  getAndroidSmsInfo() {}

  /**
   * Attempts the phone hub notification access setup flow.
   */
  attemptNotificationSetup() {}

  /**
   * Cancels the phone hub notification access setup flow.
   */
  cancelNotificationSetup() {}

  /**
   * Attempts the phone hub apps access setup flow.
   */
  attemptAppsSetup() {}

  /**
   * Cancels the phone hub apps access setup flow.
   */
  cancelAppsSetup() {}

  /**
   * Attempts the phone hub combined feature access setup flow.
   */
  attemptCombinedFeatureSetup(cameraRoll, notifications) {}

  /**
   * Cancels the phone hub combined feature access setup flow.
   */
  cancelCombinedFeatureSetup() {}

  /**
   * Attempts to connect to the phone before setup.
   */
  attemptFeatureSetupConnection() {}

  /**
   * Cancel the connection flow.
   */
  cancelFeatureSetupConnection() {}

  /**
   * Log [Cancel] button click event in phone hub combined feature access setup
   * flow.
   *  @param {!PhoneHubPermissionsSetupFlowScreens} screen
   *  @param {!PhoneHubPermissionsSetupAction} action
   */
  logPhoneHubPermissionSetUpScreenAction(screen, action) {}

  /**
   * Log phone hub combined feature access [Set Up] button click event.
   *  @param {!PhoneHubPermissionsSetupFeatureCombination} setup_mode
   */
  logPhoneHubPermissionSetUpButtonClicked(setup_mode) {}

  /**
   * Log setup mode when multidevice permissions set up intro screen is
   * displayed.
   * @param {!PhoneHubPermissionsSetupFeatureCombination} setup_mode
   */
  logPhoneHubPermissionOnboardingSetupMode(setup_mode) {}

  /**
   * Log setup mode when multidevice permissions set up finished screen is
   * displayed.
   * @param {!PhoneHubPermissionsSetupFeatureCombination} completed_mode
   */
  logPhoneHubPermissionOnboardingSetupResult(completed_mode) {}
}

/** @type {?MultiDeviceBrowserProxy} */
let instance = null;

/**
 * @implements {MultiDeviceBrowserProxy}
 */
export class MultiDeviceBrowserProxyImpl {
  /** @return {!MultiDeviceBrowserProxy} */
  static getInstance() {
    return instance || (instance = new MultiDeviceBrowserProxyImpl());
  }

  /** @param {!MultiDeviceBrowserProxy} obj */
  static setInstanceForTesting(obj) {
    instance = obj;
  }

  /** @override */
  showMultiDeviceSetupDialog() {
    chrome.send('showMultiDeviceSetupDialog');
  }

  /** @override */
  getPageContentData() {
    return sendWithPromise('getPageContentData');
  }

  /** @override */
  setFeatureEnabledState(feature, enabled, opt_authToken) {
    return sendWithPromise(
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
    return sendWithPromise('getSmartLockSignInEnabled');
  }

  /** @override */
  setSmartLockSignInEnabled(enabled, opt_authToken) {
    chrome.send('setSmartLockSignInEnabled', [enabled, opt_authToken]);
  }

  /** @override */
  getSmartLockSignInAllowed() {
    return sendWithPromise('getSmartLockSignInAllowed');
  }

  /** @override */
  getAndroidSmsInfo() {
    return sendWithPromise('getAndroidSmsInfo');
  }

  /** @override */
  attemptNotificationSetup() {
    chrome.send('attemptNotificationSetup');
  }

  /** @override */
  cancelNotificationSetup() {
    chrome.send('cancelNotificationSetup');
  }

  /** @override */
  attemptAppsSetup() {
    chrome.send('attemptAppsSetup');
  }

  /** @override */
  cancelAppsSetup() {
    chrome.send('cancelAppsSetup');
  }

  /** @override */
  attemptCombinedFeatureSetup(cameraRoll, notifications) {
    chrome.send('attemptCombinedFeatureSetup', [cameraRoll, notifications]);
  }

  /** @override */
  cancelCombinedFeatureSetup() {
    chrome.send('cancelCombinedFeatureSetup');
  }

  /** @override */
  attemptFeatureSetupConnection() {
    chrome.send('attemptFeatureSetupConnection');
  }

  /** @override */
  cancelFeatureSetupConnection() {
    chrome.send('cancelFeatureSetupConnection');
  }

  /** @override */
  logPhoneHubPermissionSetUpScreenAction(screen, action) {
    chrome.send('logPhoneHubPermissionSetUpScreenAction', [screen, action]);
  }

  /** @override */
  logPhoneHubPermissionSetUpButtonClicked(setup_mode) {
    chrome.send('logPhoneHubPermissionSetUpButtonClicked', [setup_mode]);
  }

  /** @override */
  logPhoneHubPermissionOnboardingSetupMode(setup_mode) {
    chrome.send('logPhoneHubPermissionOnboardingSetupMode', [setup_mode]);
  }

  /** @override */
  logPhoneHubPermissionOnboardingSetupResult(completed_mode) {
    chrome.send('logPhoneHubPermissionOnboardingSetupResult', [completed_mode]);
  }
}
