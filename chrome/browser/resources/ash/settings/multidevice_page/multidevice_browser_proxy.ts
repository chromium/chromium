// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

import {MultiDeviceFeature, MultiDevicePageContentData, PhoneHubPermissionsSetupAction, PhoneHubPermissionsSetupFeatureCombination, PhoneHubPermissionsSetupFlowScreens} from './multidevice_constants.js';


export interface MultiDeviceBrowserProxy {
  showMultiDeviceSetupDialog(): void;

  getPageContentData(): Promise<MultiDevicePageContentData>;

  /**
   * @param feature The feature whose state  should be set.
   * @param enabled Whether the feature should be turned off or on.
   * @param authToken Proof that the user is authenticated.
   *     Needed to enable Smart Lock, and Better Together Suite if the Smart
   *     Lock user pref is enabled.
   * @return Whether the operation was successful.
   */
  setFeatureEnabledState(
      feature: MultiDeviceFeature, enabled: boolean,
      authToken?: string): Promise<boolean>;

  removeHostDevice(): void;

  retryPendingHostSetup(): void;

  /**
   * Attempts the phone hub notification access setup flow.
   */
  attemptNotificationSetup(): void;

  /**
   * Cancels the phone hub notification access setup flow.
   */
  cancelNotificationSetup(): void;

  /**
   * Attempts the phone hub apps access setup flow.
   */
  attemptAppsSetup(): void;

  /**
   * Cancels the phone hub apps access setup flow.
   */
  cancelAppsSetup(): void;

  /**
   * Attempts the phone hub combined feature access setup flow.
   */
  attemptCombinedFeatureSetup(
      showCameraRoll: boolean, showNotifications: boolean): void;

  /**
   * Cancels the phone hub combined feature access setup flow.
   */
  cancelCombinedFeatureSetup(): void;

  /**
   * Attempts to connect to the phone before setup.
   */
  attemptFeatureSetupConnection(): void;

  /**
   * Cancel the connection flow.
   */
  cancelFeatureSetupConnection(): void;

  /**
   * Open the Chrome Sync settings page in browser settings.
   */
  showBrowserSyncSettings(): void;

  /**
   * Log [Cancel] button click event in phone hub combined feature access setup
   * flow.
   */
  logPhoneHubPermissionSetUpScreenAction(
      screen: PhoneHubPermissionsSetupFlowScreens,
      action: PhoneHubPermissionsSetupAction): void;

  /**
   * Log phone hub combined feature access [Set Up] button click event.
   */
  logPhoneHubPermissionSetUpButtonClicked(
      setupMode: PhoneHubPermissionsSetupFeatureCombination): void;

  /**
   * Log setup mode when multidevice permissions set up intro screen is
   * displayed.
   */
  logPhoneHubPermissionOnboardingSetupMode(
      setupMode: PhoneHubPermissionsSetupFeatureCombination): void;

  /**
   * Log setup mode when multidevice permissions set up finished screen is
   * displayed.
   */
  logPhoneHubPermissionOnboardingSetupResult(
      completedMode: PhoneHubPermissionsSetupFeatureCombination): void;
}

let instance: MultiDeviceBrowserProxy|null = null;

export class MultiDeviceBrowserProxyImpl implements MultiDeviceBrowserProxy {
  static getInstance(): MultiDeviceBrowserProxy {
    return instance || (instance = new MultiDeviceBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: MultiDeviceBrowserProxy): void {
    instance = obj;
  }

  showMultiDeviceSetupDialog(): void {
    chrome.send('showMultiDeviceSetupDialog');
  }

  getPageContentData(): Promise<MultiDevicePageContentData> {
    return sendWithPromise('getPageContentData');
  }

  setFeatureEnabledState(
      feature: MultiDeviceFeature, enabled: boolean,
      authToken?: string): Promise<boolean> {
    return sendWithPromise(
        'setFeatureEnabledState', feature, enabled, authToken);
  }

  removeHostDevice(): void {
    chrome.send('removeHostDevice');
  }

  retryPendingHostSetup(): void {
    chrome.send('retryPendingHostSetup');
  }

  attemptNotificationSetup(): void {
    chrome.send('attemptNotificationSetup');
  }

  cancelNotificationSetup(): void {
    chrome.send('cancelNotificationSetup');
  }

  attemptAppsSetup(): void {
    chrome.send('attemptAppsSetup');
  }

  cancelAppsSetup(): void {
    chrome.send('cancelAppsSetup');
  }

  attemptCombinedFeatureSetup(
      showCameraRoll: boolean, showNotifications: boolean): void {
    chrome.send(
        'attemptCombinedFeatureSetup', [showCameraRoll, showNotifications]);
  }

  cancelCombinedFeatureSetup(): void {
    chrome.send('cancelCombinedFeatureSetup');
  }

  attemptFeatureSetupConnection(): void {
    chrome.send('attemptFeatureSetupConnection');
  }

  cancelFeatureSetupConnection(): void {
    chrome.send('cancelFeatureSetupConnection');
  }

  showBrowserSyncSettings(): void {
    chrome.send('showBrowserSyncSettings');
  }

  logPhoneHubPermissionSetUpScreenAction(
      screen: PhoneHubPermissionsSetupFlowScreens,
      action: PhoneHubPermissionsSetupAction): void {
    chrome.send('logPhoneHubPermissionSetUpScreenAction', [screen, action]);
  }

  logPhoneHubPermissionSetUpButtonClicked(
      setupMode: PhoneHubPermissionsSetupFeatureCombination): void {
    chrome.send('logPhoneHubPermissionSetUpButtonClicked', [setupMode]);
  }

  logPhoneHubPermissionOnboardingSetupMode(
      setupMode: PhoneHubPermissionsSetupFeatureCombination): void {
    chrome.send('logPhoneHubPermissionOnboardingSetupMode', [setupMode]);
  }

  logPhoneHubPermissionOnboardingSetupResult(
      completedMode: PhoneHubPermissionsSetupFeatureCombination): void {
    chrome.send('logPhoneHubPermissionOnboardingSetupResult', [completedMode]);
  }
}
