// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-multidevice-combined-setup-item' encapsulates
 * special logic for setting up multiple features from one click.
 */

import 'chrome://resources/cr_components/localized_link/localized_link.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './multidevice_feature_item.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MultiDeviceBrowserProxy, MultiDeviceBrowserProxyImpl} from './multidevice_browser_proxy.js';
import {PhoneHubPermissionsSetupFeatureCombination} from './multidevice_constants.js';
import {MultiDeviceFeatureBehavior, MultiDeviceFeatureBehaviorInterface} from './multidevice_feature_behavior.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {MultiDeviceFeatureBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 */
const SettingsMultideviceCombinedSetupItemElementBase =
    mixinBehaviors([MultiDeviceFeatureBehavior, I18nBehavior], PolymerElement);

/** @polymer */
class SettingsMultideviceCombinedSetupItemElement extends
    SettingsMultideviceCombinedSetupItemElementBase {
  static get is() {
    return 'settings-multidevice-combined-setup-item';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
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
    };
  }

  /** @override */
  constructor() {
    super();

    /** @private {!MultiDeviceBrowserProxy} */
    this.browserProxy_ = MultiDeviceBrowserProxyImpl.getInstance();
  }

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
  }

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
  }

  /** @private */
  handlePhoneHubSetupClick_() {
    const permissionSetupRequiredEvent = new CustomEvent(
        'permission-setup-requested', {bubbles: true, composed: true});
    this.dispatchEvent(permissionSetupRequiredEvent);

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
  }

  /**
   * @return {boolean}
   * @private
   */
  getButtonDisabledState_() {
    return !this.isSuiteOn() || !this.isPhoneHubOn();
  }
}

customElements.define(
    SettingsMultideviceCombinedSetupItemElement.is,
    SettingsMultideviceCombinedSetupItemElement);
