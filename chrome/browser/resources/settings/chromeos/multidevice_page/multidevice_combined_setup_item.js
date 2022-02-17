// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-multidevice-combined-setup-item' encapsulates
 * special logic for setting up multiple features from one click.
 */
Polymer({
  is: 'settings-multidevice-combined-setup-item',

  behaviors: [MultiDeviceFeatureBehavior],

  properties: {
    /** Whether this item should show Camera Roll info */
    cameraRoll: {
      type: Boolean,
      value: false,
    },

    /** Whether this item should show Notifications info */
    notifications: {
      type: Boolean,
      value: false,
    },

    /** Whether this item should show App Streaming info */
    appStreaming: {
      type: Boolean,
      value: false,
    },

    /** @private */
    setupName_: {
      type: String,
      computed: 'getSetupName_(cameraRoll, notifications, appStreaming)',
      reflectToAttribute: true,
    },

    /** @private */
    setupSummary_: {
      type: String,
      computed: 'getSetupSummary_(cameraRoll, notifications, appStreaming)',
      reflectToAttribute: true,
    },
  },

  /**
   * @return {string}
   * @private
   */
  getSetupName_() {
    if (this.cameraRoll && this.notifications && this.appStreaming) {
      return this.i18n(
          'multidevicePhoneHubCameraRollNotificationsAndAppsItemTitle');
    }
    if (this.cameraRoll && this.notifications) {
      return this.i18n(
          'multidevicePhoneHubCameraRollAndNotificationsItemTitle');
    }
    if (this.cameraRoll && this.appStreaming) {
      return this.i18n('multidevicePhoneHubCameraRollAndAppsItemTitle');
    }
    if (this.notifications && this.appStreaming) {
      return this.i18n('multidevicePhoneHubAppsAndNotificationsItemTitle');
    }
    return '';
  },

  /**
   * @return {string}
   * @private
   */
  getSetupSummary_() {
    if (this.cameraRoll && this.notifications && this.appStreaming) {
      return this.i18n(
          'multidevicePhoneHubCameraRollNotificationsAndAppsItemSummary');
    }
    if (this.cameraRoll && this.notifications) {
      return this.i18n(
          'multidevicePhoneHubCameraRollAndNotificationsItemSummary');
    }
    if (this.cameraRoll && this.appStreaming) {
      return this.i18n('multidevicePhoneHubCameraRollAndAppsItemSummary');
    }
    if (this.notifications && this.appStreaming) {
      return this.i18n('multidevicePhoneHubAppsAndNotificationsItemSummary');
    }
    return '';
  },

  /** @private */
  handlePhoneHubCombinedSetupClick_() {
    this.fire(
        'permission-setup-requested',
        {mode: PhoneHubPermissionsSetupMode.ALL_PERMISSIONS_SETUP_MODE});
  },

  /**
   * @return {boolean}
   * @private
   */
  getButtonDisabledState_() {
    return !this.isSuiteOn() || !this.isPhoneHubOn();
  },
});
