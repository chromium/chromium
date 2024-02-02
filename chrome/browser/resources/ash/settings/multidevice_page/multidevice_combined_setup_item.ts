// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-multidevice-combined-setup-item' encapsulates
 * special logic for setting up multiple features from one click.
 */

import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import './multidevice_feature_item.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MultiDeviceBrowserProxy, MultiDeviceBrowserProxyImpl} from './multidevice_browser_proxy.js';
import {getTemplate} from './multidevice_combined_setup_item.html.js';
import {PhoneHubPermissionsSetupFeatureCombination} from './multidevice_constants.js';
import {MultiDeviceFeatureMixin} from './multidevice_feature_mixin.js';

const SettingsMultideviceCombinedSetupItemElementBase =
    MultiDeviceFeatureMixin(PolymerElement);

export class SettingsMultideviceCombinedSetupItemElement extends
    SettingsMultideviceCombinedSetupItemElementBase {
  static get is() {
    return 'settings-multidevice-combined-setup-item' as const;
  }

  static get template() {
    return getTemplate();
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

      setupName_: {
        type: String,
        computed: 'getSetupName_(cameraRoll, notifications, appStreaming)',
        reflectToAttribute: true,
      },

      setupSummary_: {
        type: String,
        computed: 'getSetupSummary_(cameraRoll, notifications, appStreaming)',
        reflectToAttribute: true,
      },
    };
  }

  cameraRoll: boolean;
  notifications: boolean;
  appStreaming: boolean;

  private browserProxy_: MultiDeviceBrowserProxy;
  private setupName_: string;
  private setupSummary_: string;

  constructor() {
    super();

    this.browserProxy_ = MultiDeviceBrowserProxyImpl.getInstance();
  }

  private getSetupName_(): string {
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

  private getSetupSummary_(): string {
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

  private handlePhoneHubSetupClick_(): void {
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

  private getButtonDisabledState_(): boolean {
    return !this.isSuiteOn() || !this.isPhoneHubOn();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsMultideviceCombinedSetupItemElement.is]:
        SettingsMultideviceCombinedSetupItemElement;
  }
  interface HTMLElementEventMap {
    'permission-setup-requested': CustomEvent;
  }
}

customElements.define(
    SettingsMultideviceCombinedSetupItemElement.is,
    SettingsMultideviceCombinedSetupItemElement);
