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
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../../settings_shared.css.js';
import '../../settings_vars.css.js';
import './multidevice_combined_setup_item.js';
import './multidevice_feature_item.js';
import './multidevice_feature_toggle.js';
import './multidevice_task_continuation_item.js';
import './multidevice_tether_item.js';
import './multidevice_wifi_sync_item.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {Route} from '../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {OsSettingsRoutes} from '../os_settings_routes.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

import {MultiDeviceBrowserProxy, MultiDeviceBrowserProxyImpl} from './multidevice_browser_proxy.js';
import {MultiDeviceFeature, MultiDeviceFeatureState, MultiDeviceSettingsMode, PhoneHubFeatureAccessProhibitedReason, PhoneHubPermissionsSetupFeatureCombination} from './multidevice_constants.js';
import {MultiDeviceFeatureBehavior, MultiDeviceFeatureBehaviorInterface} from './multidevice_feature_behavior.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {MultiDeviceFeatureBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 */
const SettingsMultideviceSubpageElementBase = mixinBehaviors(
    [
      DeepLinkingBehavior,
      MultiDeviceFeatureBehavior,
      RouteObserverBehavior,
      I18nBehavior,
    ],
    PolymerElement);

/** @polymer */
class SettingsMultideviceSubpageElement extends
    SettingsMultideviceSubpageElementBase {
  static get is() {
    return 'settings-multidevice-subpage';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
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
       * @type {!Set<!Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
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

  /** @override */
  constructor() {
    super();

    /** @private {!MultiDeviceBrowserProxy} */
    this.browserProxy_ = MultiDeviceBrowserProxyImpl.getInstance();
  }

  /**
   * @param {!Route} route
   * @param {!Route=} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== routes.MULTIDEVICE_FEATURES) {
      return;
    }

    this.attemptDeepLink();
  }

  /** @private */
  handleVerifyButtonClick_(event) {
    this.browserProxy_.retryPendingHostSetup();
  }

  /** @private */
  handleAndroidMessagesButtonClick_() {
    this.browserProxy_.setUpAndroidSms();
  }

  /**
   * @return {boolean}
   * @private
   */
  shouldShowIndividualFeatures_() {
    return this.pageContentData.mode ===
        MultiDeviceSettingsMode.HOST_SET_VERIFIED;
  }

  /**
   * @return {boolean}
   * @private
   */
  shouldShowVerifyButton_() {
    return [
      MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER,
      MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION,
    ].includes(this.pageContentData.mode);
  }

  /**
   * @return {boolean}
   * @private
   */
  shouldShowSuiteToggle_() {
    return this.pageContentData.mode ===
        MultiDeviceSettingsMode.HOST_SET_VERIFIED;
  }

  /** @private */
  handleForgetDeviceClick_() {
    this.$.forgetDeviceDialog.showModal();
  }

  /** @private */
  onForgetDeviceDialogCancelClick_() {
    this.$.forgetDeviceDialog.close();
  }

  /** @private */
  onForgetDeviceDialogConfirmClick_() {
    const forgetDeviceRequestedEvent =
        new CustomEvent('forget-device-requested', {
          bubbles: true,
          composed: true,
        });
    this.dispatchEvent(forgetDeviceRequestedEvent);
    this.$.forgetDeviceDialog.close();
  }

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
  }

  /**
   * @return {boolean}
   * @private
   */
  doesAndroidMessagesRequireSetUp_() {
    return this.getFeatureState(MultiDeviceFeature.MESSAGES) ===
        MultiDeviceFeatureState.FURTHER_SETUP_REQUIRED;
  }

  /**
   * @return {boolean}
   * @private
   */
  isAndroidMessagesSetupButtonDisabled_() {
    const messagesFeatureState =
        this.getFeatureState(MultiDeviceFeature.MESSAGES);
    return !this.isSuiteOn() ||
        messagesFeatureState === MultiDeviceFeatureState.PROHIBITED_BY_POLICY;
  }

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
        return this.i18n('multideviceItemDisabledByPhoneAdminTooltip');
      default:
        return this.i18n('multideviceNotificationAccessProhibitedTooltip');
    }
  }

  getPhoneHubAppsTooltip_() {
    if (!this.isFeatureAllowedByPolicy(MultiDeviceFeature.ECHE)) {
      return '';
    }
    if (!this.isPhoneHubAppsAccessProhibited()) {
      return '';
    }
    return this.i18n('multideviceItemDisabledByPhoneAdminTooltip');
  }

  /**
   * @return {boolean}
   * @private
   */
  shouldShowPhoneHubCameraRollItem_() {
    return this.isFeatureSupported(MultiDeviceFeature.PHONE_HUB_CAMERA_ROLL) &&
        (!this.isPhoneHubCameraRollSetupRequired() ||
         !this.shouldShowPhoneHubCombinedSetupItem_());
  }

  /**
   * @return {boolean}
   * @private
   */
  shouldShowPhoneHubNotificationsItem_() {
    return this.isFeatureSupported(
               MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS) &&
        (!this.isPhoneHubNotificationsSetupRequired() ||
         !this.shouldShowPhoneHubCombinedSetupItem_());
  }

  /**
   * @return {boolean}
   * @private
   */
  shouldShowPhoneHubAppsItem_() {
    return this.isFeatureSupported(MultiDeviceFeature.ECHE) &&
        (!this.isPhoneHubAppsSetupRequired() ||
         !this.shouldShowPhoneHubCombinedSetupItem_());
  }

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
  }

  /** @private */
  handleNotificationSetupClicked_() {
    this.handlePhoneHubSetupClick(
        PhoneHubPermissionsSetupFeatureCombination.NOTIFICATION);
  }

  /** @private */
  handleCameraRollSetupClicked_() {
    this.handlePhoneHubSetupClick(
        PhoneHubPermissionsSetupFeatureCombination.CAMERA_ROLL);
  }

  /** @private */
  handleMessagingAppSetupClicked_() {
    this.handlePhoneHubSetupClick(
        PhoneHubPermissionsSetupFeatureCombination.MESSAGING_APP);
  }

  /** @param {!PhoneHubPermissionsSetupFeatureCombination} setupMode */
  /** @private */
  handlePhoneHubSetupClick(setupMode) {
    const permissionSetupRequestedEvent = new CustomEvent(
        'permission-setup-requested', {bubbles: true, composed: true});
    this.dispatchEvent(permissionSetupRequestedEvent);
    this.browserProxy_.logPhoneHubPermissionSetUpButtonClicked(setupMode);
  }

  /**
   * @return {boolean}
   * @private
   */
  isPhoneHubDisabled_() {
    return !this.isSuiteOn() || !this.isPhoneHubOn();
  }
}

customElements.define(
    SettingsMultideviceSubpageElement.is, SettingsMultideviceSubpageElement);
