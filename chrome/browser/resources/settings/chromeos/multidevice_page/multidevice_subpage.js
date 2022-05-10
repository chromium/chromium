// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_link_row/cr_link_row.js';
import '//resources/cr_elements/shared_vars_css.m.js';
import '../../settings_shared_css.js';
import '../../settings_vars_css.js';
import './multidevice_combined_setup_item.js';
import './multidevice_feature_item.js';
import './multidevice_feature_toggle.js';
import './multidevice_task_continuation_item.js';
import './multidevice_tether_item.js';
import './multidevice_wifi_sync_item.js';

import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {Route} from '../../router.js';
import {DeepLinkingBehavior} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {OsSettingsRoutes} from '../os_settings_routes.js';
import {RouteObserverBehavior} from '../route_observer_behavior.js';

import {MultiDeviceBrowserProxy, MultiDeviceBrowserProxyImpl} from './multidevice_browser_proxy.js';
import {MultiDeviceFeature, MultiDeviceFeatureState, MultiDeviceSettingsMode, PhoneHubFeatureAccessProhibitedReason, PhoneHubPermissionsSetupFeatureCombination} from './multidevice_constants.js';
import {MultiDeviceFeatureBehavior} from './multidevice_feature_behavior.js';

/**
 * @fileoverview
 * Subpage of settings-multidevice-page for managing multidevice features
 * individually and for forgetting a host.
 */
Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-multidevice-subpage',

  behaviors: [
    DeepLinkingBehavior,
    MultiDeviceFeatureBehavior,
    RouteObserverBehavior,
  ],

  properties: {
    /**
     * Alias for allowing Polymer bindings to routes.
     * @type {?OsSettingsRoutes}
     */
    routes: {
      type: Object,
      value: routes,
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kInstantTetheringOnOff,
        chromeos.settings.mojom.Setting.kMultiDeviceOnOff,
        chromeos.settings.mojom.Setting.kSmartLockOnOff,
        chromeos.settings.mojom.Setting.kMessagesSetUp,
        chromeos.settings.mojom.Setting.kMessagesOnOff,
        chromeos.settings.mojom.Setting.kForgetPhone,
        chromeos.settings.mojom.Setting.kPhoneHubOnOff,
        chromeos.settings.mojom.Setting.kPhoneHubCameraRollOnOff,
        chromeos.settings.mojom.Setting.kPhoneHubNotificationsOnOff,
        chromeos.settings.mojom.Setting.kPhoneHubTaskContinuationOnOff,
        chromeos.settings.mojom.Setting.kWifiSyncOnOff,
        chromeos.settings.mojom.Setting.kPhoneHubAppsOnOff,
      ]),
    },
  },

  /** @private {?MultiDeviceBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = MultiDeviceBrowserProxyImpl.getInstance();
  },

  /**
   * @param {!Route} route
   * @param {!Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== routes.MULTIDEVICE_FEATURES) {
      return;
    }

    this.attemptDeepLink();
  },

  /** @private */
  handleVerifyButtonClick_(event) {
    this.browserProxy_.retryPendingHostSetup();
  },

  /** @private */
  handleAndroidMessagesButtonClick_() {
    this.browserProxy_.setUpAndroidSms();
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowIndividualFeatures_() {
    return this.pageContentData.mode ===
        MultiDeviceSettingsMode.HOST_SET_VERIFIED;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowVerifyButton_() {
    return [
      MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER,
      MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION,
    ].includes(this.pageContentData.mode);
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowSuiteToggle_() {
    return this.pageContentData.mode ===
        MultiDeviceSettingsMode.HOST_SET_VERIFIED;
  },

  /** @private */
  handleForgetDeviceClick_() {
    this.$.forgetDeviceDialog.showModal();
  },

  /** @private */
  onForgetDeviceDialogCancelClick_() {
    this.$.forgetDeviceDialog.close();
  },

  /** @private */
  onForgetDeviceDialogConfirmClick_() {
    this.fire('forget-device-requested');
    this.$.forgetDeviceDialog.close();
  },

  /**
   * @return {string}
   * @private
   */
  getStatusInnerHtml_() {
    if ([
          MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER,
          MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION,
        ].includes(this.pageContentData.mode)) {
      return this.i18nAdvanced('multideviceVerificationText');
    }
    return this.isSuiteOn() ? this.i18n('multideviceEnabled') :
                              this.i18n('multideviceDisabled');
  },

  /**
   * @return {boolean}
   * @private
   */
  doesAndroidMessagesRequireSetUp_() {
    return this.getFeatureState(MultiDeviceFeature.MESSAGES) ===
        MultiDeviceFeatureState.FURTHER_SETUP_REQUIRED;
  },

  /**
   * @return {boolean}
   * @private
   */
  isAndroidMessagesSetupButtonDisabled_() {
    const messagesFeatureState =
        this.getFeatureState(MultiDeviceFeature.MESSAGES);
    return !this.isSuiteOn() ||
        messagesFeatureState === MultiDeviceFeatureState.PROHIBITED_BY_POLICY;
  },

  getPhoneHubNotificationsTooltip_() {
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
        return this.i18n(
            'multideviceNotificationAccessProhibitedDisabledByAdminTooltip');
      default:
        return this.i18n('multideviceNotificationAccessProhibitedTooltip');
    }
  },

  getPhoneHubAppsTooltip_() {
    if (!this.isFeatureAllowedByPolicy(MultiDeviceFeature.ECHE)) {
      return '';
    }
    if (!this.isPhoneHubAppsAccessProhibited()) {
      return '';
    }
    return this.i18n('multideviceAppsAccessProhibitedDisabledByAdminTooltip');
  },

  /**
   * TODO(b/227674947): Delete method when Sign in with Smart Lock is removed.
   * If Smart Lock Sign in is removed there is no subpage to navigate to, so we
   * set the subpageRoute to undefined.
   * @return {undefined | Object}
   * @private
   */
  getSmartLockSubpageRoute_() {
    return loadTimeData.getBoolean('isSmartLockSignInRemoved') ?
        undefined :
        routes.SMART_LOCK;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowPhoneHubCameraRollItem_() {
    return this.isFeatureSupported(MultiDeviceFeature.PHONE_HUB_CAMERA_ROLL) &&
        (!this.isPhoneHubCameraRollSetupRequired() ||
         !this.shouldShowPhoneHubCombinedSetupItem_());
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowPhoneHubNotificationsItem_() {
    return this.isFeatureSupported(
               MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS) &&
        (!this.isPhoneHubNotificationsSetupRequired() ||
         !this.shouldShowPhoneHubCombinedSetupItem_());
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowPhoneHubAppsItem_() {
    return this.isFeatureSupported(MultiDeviceFeature.ECHE) &&
        (!this.isPhoneHubAppsSetupRequired() ||
         !this.shouldShowPhoneHubCombinedSetupItem_());
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowPhoneHubCombinedSetupItem_() {
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
  },

  /** @private */
  handlePhoneHubSetupClick_() {
    this.fire('permission-setup-requested');
    let setupMode = PhoneHubPermissionsSetupFeatureCombination.NONE;
    if (this.shouldShowPhoneHubCameraRollItem_()) {
      setupMode = PhoneHubPermissionsSetupFeatureCombination.CAMERA_ROLL;
    }
    if (this.shouldShowPhoneHubNotificationsItem_()) {
      setupMode = PhoneHubPermissionsSetupFeatureCombination.NOTIFICATION;
    }
    if (this.shouldShowPhoneHubAppsItem_()) {
      setupMode = PhoneHubPermissionsSetupFeatureCombination.MESSAGING_APP;
    }
    this.browserProxy_.logPhoneHubPermissionSetUpButtonClicked(setupMode);
  },

  /**
   * @return {boolean}
   * @private
   */
  isPhoneHubDisabled_() {
    return !this.isSuiteOn() || !this.isPhoneHubOn();
  },
});
