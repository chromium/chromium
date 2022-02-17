// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Subpage of settings-multidevice-page for managing multidevice features
 * individually and for forgetting a host.
 */
Polymer({
  is: 'settings-multidevice-subpage',

  behaviors: [
    DeepLinkingBehavior,
    MultiDeviceFeatureBehavior,
    settings.RouteObserverBehavior,
  ],

  properties: {
    /**
     * Alias for allowing Polymer bindings to settings.routes.
     * @type {?OsSettingsRoutes}
     */
    routes: {
      type: Object,
      value: settings.routes,
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

  /** @private {?settings.MultiDeviceBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = settings.MultiDeviceBrowserProxyImpl.getInstance();
  },

  /**
   * @param {!settings.Route} route
   * @param {!settings.Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== settings.routes.MULTIDEVICE_FEATURES) {
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
        settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowVerifyButton_() {
    return [
      settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER,
      settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION,
    ].includes(this.pageContentData.mode);
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowSuiteToggle_() {
    return this.pageContentData.mode ===
        settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED;
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
          settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER,
          settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION,
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
    return this.getFeatureState(settings.MultiDeviceFeature.MESSAGES) ===
        settings.MultiDeviceFeatureState.FURTHER_SETUP_REQUIRED;
  },

  /**
   * @return {boolean}
   * @private
   */
  isAndroidMessagesSetupButtonDisabled_() {
    const messagesFeatureState =
        this.getFeatureState(settings.MultiDeviceFeature.MESSAGES);
    return !this.isSuiteOn() ||
        messagesFeatureState ===
        settings.MultiDeviceFeatureState.PROHIBITED_BY_POLICY;
  },

  getPhoneHubNotificationsTooltip_() {
    if (!this.isPhoneHubNotificationAccessProhibited()) {
      return '';
    }

    switch (this.pageContentData.notificationAccessProhibitedReason) {
      case settings.PhoneHubNotificationAccessProhibitedReason.UNKNOWN:
        return this.i18n('multideviceNotificationAccessProhibitedTooltip');
      case settings.PhoneHubNotificationAccessProhibitedReason.WORK_PROFILE:
        return this.i18n('multideviceNotificationAccessProhibitedTooltip');
      case settings.PhoneHubNotificationAccessProhibitedReason
          .DISABLED_BY_PHONE_POLICY:
        return this.i18n(
            'multideviceNotificationAccessProhibitedDisabledByAdminTooltip');
      default:
        return this.i18n('multideviceNotificationAccessProhibitedTooltip');
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  isPhoneHubCameraRollSetupRequired_() {
    return this.isFeatureSupported(
               settings.MultiDeviceFeature.PHONE_HUB_CAMERA_ROLL) &&
        !this.pageContentData.isCameraRollFilePermissionGranted;
  },

  /**
   * @return {boolean}
   * @private
   */
  isPhoneHubAppsSetupRequired_() {
    return this.isFeatureSupported(settings.MultiDeviceFeature.ECHE) &&
        this.pageContentData.isPhoneHubPermissionsDialogSupported &&
        !this.pageContentData.isPhoneHubAppsAccessGranted;
  },

  /**
   * @return {boolean}
   * @private
   */
  isPhoneHubNotificationsSetupRequired_() {
    return this.pageContentData.notificationAccessStatus ===
        settings.PhoneHubNotificationAccessStatus.AVAILABLE_BUT_NOT_GRANTED;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowPhoneHubCameraRollItem_() {
    return this.isFeatureSupported(
               settings.MultiDeviceFeature.PHONE_HUB_CAMERA_ROLL) &&
        (!this.isPhoneHubCameraRollSetupRequired_() ||
         !this.shouldShowPhoneHubCombinedSetupItem_());
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowPhoneHubNotificationsItem_() {
    return this.isFeatureSupported(
               settings.MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS) &&
        (!this.isPhoneHubNotificationsSetupRequired_() ||
         !this.shouldShowPhoneHubCombinedSetupItem_());
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowPhoneHubAppsItem_() {
    return this.isFeatureSupported(settings.MultiDeviceFeature.ECHE) &&
        (!this.isPhoneHubAppsSetupRequired_() ||
         !this.shouldShowPhoneHubCombinedSetupItem_());
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowPhoneHubCombinedSetupItem_() {
    let numFeaturesSetupRequired = 0;
    if (this.isPhoneHubCameraRollSetupRequired_()) {
      numFeaturesSetupRequired++;
    }
    if (this.isPhoneHubNotificationsSetupRequired_()) {
      numFeaturesSetupRequired++;
    }
    if (this.isPhoneHubAppsSetupRequired_()) {
      numFeaturesSetupRequired++;
    }
    return numFeaturesSetupRequired >= 2;
  },

  /** @private */
  handlePhoneHubCameraRollSetupClick_() {
    this.fire(
        'permission-setup-requested',
        {mode: PhoneHubPermissionsSetupMode.CAMERA_ROLL_SETUP_MODE});
  },

  /** @private */
  handlePhoneHubNotificationsSetupClick_() {
    this.fire(
        'permission-setup-requested',
        {mode: PhoneHubPermissionsSetupMode.NOTIFICATION_SETUP_MODE});
  },

  /** @private */
  handlePhoneHubAppsSetupClick_() {
    this.fire(
        'permission-setup-requested',
        {mode: PhoneHubPermissionsSetupMode.APPS_SETUP_MODE});
  },

  /**
   * @return {boolean}
   * @private
   */
  isPhoneHubDisabled_() {
    return !this.isSuiteOn() || !this.isPhoneHubOn();
  },
});
