// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/localized_link/localized_link.js';
import '//resources/cr_elements/cr_button/cr_button.m.js';
import './multidevice_feature_item.js';

import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MultiDeviceBrowserProxy, MultiDeviceBrowserProxyImpl} from './multidevice_browser_proxy.js';
import {PhoneHubPermissionsSetupFeatureCombination} from './multidevice_constants.js';
import {MultiDeviceFeatureBehavior} from './multidevice_feature_behavior.js';

/**
 * @fileoverview 'settings-multidevice-combined-setup-item' encapsulates
 * special logic for setting up multiple features from one click.
 */
Polymer({
  _template: html`{__html_template__}`,
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

  /** @private {?MultiDeviceBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = MultiDeviceBrowserProxyImpl.getInstance();
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
  handlePhoneHubSetupClick_() {
    this.fire('permission-setup-requested');
    let setupMode = PhoneHubPermissionsSetupFeatureCombination.NONE;
    if (this.cameraRoll && this.notifications && this.appStreaming) {
      setupMode = PhoneHubPermissionsSetupFeatureCombination.ALL_PERMISSONS;
    }
    if (this.cameraRoll && this.notifications) {
      setupMode = PhoneHubPermissionsSetupFeatureCombination
                      .NOTIFICATION_AND_CAMERA_ROLL;
    }
    if (this.cameraRoll && this.appStreaming) {
      setupMode = PhoneHubPermissionsSetupFeatureCombination
                      .MESSAGING_APP_AND_CAMERA_ROLL;
    }
    if (this.notifications && this.appStreaming) {
      setupMode = PhoneHubPermissionsSetupFeatureCombination
                      .NOTIFICATION_AND_MESSAGING_APP;
    }
    this.browserProxy_.logPhoneHubPermissionSetUpButtonClicked(setupMode);
  },

  /**
   * @return {boolean}
   * @private
   */
  getButtonDisabledState_() {
    return !this.isSuiteOn() || !this.isPhoneHubOn();
  },
});
