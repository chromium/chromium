// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Subpage of settings-multidevice-page for managing multidevice features
 * individually and for forgetting a host.
 */
cr.exportPath('settings');

Polymer({
  is: 'settings-multidevice-subpage',

  behaviors: [MultiDeviceFeatureBehavior],

  properties: {
    /**
     * Alias for allowing Polymer bindings to settings.routes.
     * @type {?SettingsRoutes}
     */
    routes: {
      type: Object,
      value: settings.routes,
    },
  },

  /** @private {?settings.MultiDeviceBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = settings.MultiDeviceBrowserProxyImpl.getInstance();
  },

  /** @private */
  handleVerifyButtonClick_: function(event) {
    this.browserProxy_.retryPendingHostSetup();
  },

  /** @private */
  handleAndroidMessagesButtonClick_: function() {
    this.browserProxy_.setUpAndroidSms();
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowIndividualFeatures_: function() {
    return this.pageContentData.mode ===
        settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowVerifyButton_: function() {
    return [
      settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER,
      settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION,
    ].includes(this.pageContentData.mode);
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowSuiteToggle_: function() {
    return this.pageContentData.mode ===
        settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED;
  },

  /** @private */
  handleForgetDeviceClick_: function() {
    this.$.forgetDeviceDialog.showModal();
  },

  /** @private */
  onForgetDeviceDialogCancelClick_: function() {
    this.$.forgetDeviceDialog.close();
  },

  /** @private */
  onForgetDeviceDialogConfirmClick_: function() {
    this.fire('forget-device-requested');
    this.$.forgetDeviceDialog.close();
  },

  /**
   * @return {string}
   * @private
   */
  getStatusInnerHtml_: function() {
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
  doesAndroidMessagesRequireSetUp_: function() {
    return this.getFeatureState(settings.MultiDeviceFeature.MESSAGES) ===
        settings.MultiDeviceFeatureState.FURTHER_SETUP_REQUIRED;
  },

  /**
   * @return {boolean}
   * @private
   */
  isAndroidMessagesSetupButtonDisabled_: function() {
    const messagesFeatureState =
        this.getFeatureState(settings.MultiDeviceFeature.MESSAGES);
    return !this.isSuiteOn() ||
        messagesFeatureState ===
        settings.MultiDeviceFeatureState.PROHIBITED_BY_POLICY;
  }
});
