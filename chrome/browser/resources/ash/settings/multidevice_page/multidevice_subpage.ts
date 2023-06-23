// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Subpage of settings-multidevice-page for managing multidevice features
 * individually and for forgetting a host.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../settings_shared.css.js';
import '../settings_vars.css.js';
import './multidevice_combined_setup_item.js';
import './multidevice_feature_item.js';
import './multidevice_feature_toggle.js';
import './multidevice_task_continuation_item.js';
import './multidevice_task_continuation_item_lacros.js';
import './multidevice_tether_item.js';
import './multidevice_wifi_sync_item.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route, routes} from '../router.js';

import {MultiDeviceBrowserProxy, MultiDeviceBrowserProxyImpl} from './multidevice_browser_proxy.js';
import {MultiDeviceFeature, MultiDeviceFeatureState, MultiDeviceSettingsMode, PhoneHubFeatureAccessProhibitedReason, PhoneHubPermissionsSetupFeatureCombination} from './multidevice_constants.js';
import {MultiDeviceFeatureMixin} from './multidevice_feature_mixin.js';
import {getTemplate} from './multidevice_subpage.html.js';

export interface SettingsMultideviceSubpageElement {
  $: {
    forgetDeviceDialog: CrDialogElement,
  };
}

const SettingsMultideviceSubpageElementBase = MultiDeviceFeatureMixin(
    DeepLinkingMixin(RouteObserverMixin(PolymerElement)));

export class SettingsMultideviceSubpageElement extends
    SettingsMultideviceSubpageElementBase {
  static get is() {
    return 'settings-multidevice-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kInstantTetheringOnOff,
          Setting.kMultiDeviceOnOff,
          Setting.kSmartLockOnOff,
          Setting.kMessagesSetUp,
          Setting.kMessagesOnOff,
          Setting.kForgetPhone,
          Setting.kPhoneHubOnOff,
          Setting.kPhoneHubCameraRollOnOff,
          Setting.kPhoneHubNotificationsOnOff,
          Setting.kPhoneHubTaskContinuationOnOff,
          Setting.kWifiSyncOnOff,
          Setting.kPhoneHubAppsOnOff,
        ]),
      },
    };
  }

  private browserProxy_: MultiDeviceBrowserProxy;

  constructor() {
    super();

    this.browserProxy_ = MultiDeviceBrowserProxyImpl.getInstance();
  }

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.MULTIDEVICE_FEATURES) {
      return;
    }

    this.attemptDeepLink();
  }

  private handleVerifyButtonClick_(): void {
    this.browserProxy_.retryPendingHostSetup();
  }

  private handleAndroidMessagesButtonClick_(): void {
    this.browserProxy_.setUpAndroidSms();
  }

  private shouldShowIndividualFeatures_(): boolean {
    return this.pageContentData.mode ===
        MultiDeviceSettingsMode.HOST_SET_VERIFIED;
  }

  private shouldShowVerifyButton_(): boolean {
    return [
      MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER,
      MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION,
    ].includes(this.pageContentData.mode);
  }

  private shouldShowSuiteToggle_(): boolean {
    return this.pageContentData.mode ===
        MultiDeviceSettingsMode.HOST_SET_VERIFIED;
  }

  private handleForgetDeviceClick_(): void {
    this.$.forgetDeviceDialog.showModal();
  }

  private onForgetDeviceDialogCancelClick_(): void {
    this.$.forgetDeviceDialog.close();
  }

  private onForgetDeviceDialogConfirmClick_(): void {
    const forgetDeviceRequestedEvent =
        new CustomEvent('forget-device-requested', {
          bubbles: true,
          composed: true,
        });
    this.dispatchEvent(forgetDeviceRequestedEvent);
    this.$.forgetDeviceDialog.close();
  }

  private getStatusInnerHtml_(): TrustedHTML|string {
    if ([
          MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER,
          MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION,
        ].includes(this.pageContentData.mode)) {
      return this.i18nAdvanced('multideviceVerificationText');
    }
    return this.isSuiteOn() ? this.i18n('multideviceEnabled') :
                              this.i18n('multideviceDisabled');
  }

  private doesAndroidMessagesRequireSetUp_(): boolean {
    return this.getFeatureState(MultiDeviceFeature.MESSAGES) ===
        MultiDeviceFeatureState.FURTHER_SETUP_REQUIRED;
  }

  private isAndroidMessagesSetupButtonDisabled_(): boolean {
    const messagesFeatureState =
        this.getFeatureState(MultiDeviceFeature.MESSAGES);
    return !this.isSuiteOn() ||
        messagesFeatureState === MultiDeviceFeatureState.PROHIBITED_BY_POLICY;
  }

  private getPhoneHubNotificationsTooltip_(): string {
    if (!this.isFeatureAllowedByPolicy(
            MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS)) {
      return '';
    }
    if (!this.isPhoneHubNotificationAccessProhibited()) {
      return '';
    }

    switch (this.pageContentData.notificationAccessProhibitedReason) {
      case PhoneHubFeatureAccessProhibitedReason.UNKNOWN:
        return this.i18n('multideviceNotificationAccessProhibitedTooltip');
      case PhoneHubFeatureAccessProhibitedReason.WORK_PROFILE:
        return this.i18n('multideviceNotificationAccessProhibitedTooltip');
      case PhoneHubFeatureAccessProhibitedReason.DISABLED_BY_PHONE_POLICY:
        return this.i18n('multideviceItemDisabledByPhoneAdminTooltip');
      default:
        return this.i18n('multideviceNotificationAccessProhibitedTooltip');
    }
  }

  private getPhoneHubAppsTooltip_(): string {
    if (!this.isFeatureAllowedByPolicy(MultiDeviceFeature.ECHE)) {
      return '';
    }
    if (!this.isPhoneHubAppsAccessProhibited()) {
      return '';
    }
    return this.i18n('multideviceItemDisabledByPhoneAdminTooltip');
  }

  private shouldShowPhoneHubCameraRollItem_(): boolean {
    return this.isFeatureSupported(MultiDeviceFeature.PHONE_HUB_CAMERA_ROLL) &&
        (!this.isPhoneHubCameraRollSetupRequired() ||
         !this.shouldShowPhoneHubCombinedSetupItem_());
  }

  private shouldShowPhoneHubNotificationsItem_(): boolean {
    return this.isFeatureSupported(
               MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS) &&
        (!this.isPhoneHubNotificationsSetupRequired() ||
         !this.shouldShowPhoneHubCombinedSetupItem_());
  }

  private shouldShowPhoneHubAppsItem_(): boolean {
    return this.isFeatureSupported(MultiDeviceFeature.ECHE) &&
        (!this.isPhoneHubAppsSetupRequired() ||
         !this.shouldShowPhoneHubCombinedSetupItem_());
  }

  private shouldShowPhoneHubCombinedSetupItem_(): boolean {
    let numFeaturesSetupRequired = 0;
    if (this.isPhoneHubCameraRollSetupRequired()) {
      numFeaturesSetupRequired++;
    }
    if (this.isPhoneHubNotificationsSetupRequired()) {
      numFeaturesSetupRequired++;
    }
    if (this.isPhoneHubAppsSetupRequired()) {
      numFeaturesSetupRequired++;
    }
    return numFeaturesSetupRequired >= 2;
  }

  private handleNotificationSetupClicked_(): void {
    this.handlePhoneHubSetupClick(
        PhoneHubPermissionsSetupFeatureCombination.NOTIFICATION);
  }

  private handleCameraRollSetupClicked_(): void {
    this.handlePhoneHubSetupClick(
        PhoneHubPermissionsSetupFeatureCombination.CAMERA_ROLL);
  }

  private handleMessagingAppSetupClicked_(): void {
    this.handlePhoneHubSetupClick(
        PhoneHubPermissionsSetupFeatureCombination.MESSAGING_APP);
  }

  private handlePhoneHubSetupClick(
      setupMode: PhoneHubPermissionsSetupFeatureCombination): void {
    const permissionSetupRequestedEvent = new CustomEvent(
        'permission-setup-requested', {bubbles: true, composed: true});
    this.dispatchEvent(permissionSetupRequestedEvent);
    this.browserProxy_.logPhoneHubPermissionSetUpButtonClicked(setupMode);
  }

  private isPhoneHubDisabled_(): boolean {
    return !this.isSuiteOn() || !this.isPhoneHubOn();
  }

  private isSyncedSessionSharingEnabled_(): boolean {
    return this.pageContentData.isChromeOSSyncedSessionSharingEnabled;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsMultideviceSubpageElement.is]: SettingsMultideviceSubpageElement;
  }
}

customElements.define(
    SettingsMultideviceSubpageElement.is, SettingsMultideviceSubpageElement);
