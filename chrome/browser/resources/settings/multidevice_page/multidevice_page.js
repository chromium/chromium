// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Settings page for managing MultiDevice features.
 */
cr.exportPath('settings');

Polymer({
  is: 'settings-multidevice-page',

  behaviors: [
    settings.RouteObserverBehavior,
    MultiDeviceFeatureBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /** Preferences state. */
    prefs: {type: Object},

    /**
     * A Map specifying which element should be focused when exiting a subpage.
     * The key of the map holds a settings.Route path, and the value holds a
     * query selector that identifies the desired element.
     * @private {!Map<string, string>}
     */
    focusConfig_: {
      type: Object,
      value: function() {
        const map = new Map();
        if (settings.routes.MULTIDEVICE_FEATURES)
          map.set(
              settings.routes.MULTIDEVICE_FEATURES.path,
              '#multidevice-item .subpage-arrow');
        return map;
      },
    },

    /**
     * Authentication token provided by password-prompt-dialog.
     * @private {string}
     */
    authToken_: {
      type: String,
      value: '',
    },

    /**
     * Feature which the user has requested to be enabled but could not be
     * enabled immediately because authentication (i.e., entering a password) is
     * required. This value is initialized to null, is set when the password
     * dialog is opened, and is reset to null again once the password dialog is
     * closed.
     * @private {?settings.MultiDeviceFeature}
     */
    featureToBeEnabledOnceAuthenticated_: {
      type: Number,
      value: null,
    },

    /** @private {boolean} */
    showPasswordPromptDialog_: {
      type: Boolean,
      value: false,
    },
  },

  listeners: {
    'auth-token-changed': 'onAuthTokenChanged_',
    'close': 'onDialogClose_',
    'feature-toggle-clicked': 'onFeatureToggleClicked_',
    'forget-device-requested': 'onForgetDeviceRequested_',
  },

  /** @private {?settings.MultiDeviceBrowserProxy} */
  browserProxy_: null,

  /** @override */
  ready: function() {
    this.browserProxy_ = settings.MultiDeviceBrowserProxyImpl.getInstance();

    this.addWebUIListener(
        'settings.updateMultidevicePageContentData',
        this.onPageContentDataChanged_.bind(this));

    this.browserProxy_.getPageContentData().then(
        this.onPageContentDataChanged_.bind(this));
  },

  /**
   * Overridden from settings.RouteObserverBehavior.
   * @protected
   */
  currentRouteChanged: function() {
    this.leaveNestedPageIfNoHostIsSet_();
  },

  /**
   * CSS class for the <div> containing all the text in the multidevice-item
   * <div>, i.e. the label and sublabel. If the host is set, the Better Together
   * icon appears so before the text (i.e. text div is 'middle' class).
   * @return {string}
   * @private
   */
  getMultiDeviceItemLabelBlockCssClass_: function() {
    return this.isHostSet() ? 'middle' : 'start';
  },

  /**
   * @return {string} Translated item label.
   * @private
   */
  getLabelText_: function() {
    return this.pageContentData.hostDeviceName ||
        this.i18n('multideviceSetupItemHeading');
  },

  /**
   * @return {string} Translated sublabel with a "learn more" link.
   * @private
   */
  getSubLabelInnerHtml_: function() {
    if (!this.isSuiteAllowedByPolicy())
      return this.i18nAdvanced('multideviceSetupSummary');
    switch (this.pageContentData.mode) {
      case settings.MultiDeviceSettingsMode.NO_ELIGIBLE_HOSTS:
        return this.i18nAdvanced('multideviceNoHostText');
      case settings.MultiDeviceSettingsMode.NO_HOST_SET:
        return this.i18nAdvanced('multideviceSetupSummary');
      case settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER:
      // Intentional fall-through.
      case settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION:
        return this.i18nAdvanced('multideviceVerificationText');
      default:
        return this.isSuiteOn() ? this.i18n('multideviceEnabled') :
                                  this.i18n('multideviceDisabled');
    }
  },

  /**
   * @return {string} Translated button text.
   * @private
   */
  getButtonText_: function() {
    switch (this.pageContentData.mode) {
      case settings.MultiDeviceSettingsMode.NO_HOST_SET:
        return this.i18n('multideviceSetupButton');
      case settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER:
      // Intentional fall-through.
      case settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION:
        return this.i18n('multideviceVerifyButton');
      default:
        return '';
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowButton_: function() {
    return [
      settings.MultiDeviceSettingsMode.NO_HOST_SET,
      settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER,
      settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION,
    ].includes(this.pageContentData.mode);
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowToggle_: function() {
    return this.pageContentData.mode ===
        settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED;
  },

  /**
   * Whether to show the separator bar and, if the state calls for a chevron
   * (a.k.a. subpage arrow) routing to the subpage, the chevron.
   * @return {boolean}
   * @private
   */
  shouldShowSeparatorAndSubpageArrow_: function() {
    return this.pageContentData.mode !==
        settings.MultiDeviceSettingsMode.NO_ELIGIBLE_HOSTS;
  },

  /**
   * @return {boolean}
   * @private
   */
  doesClickOpenSubpage_: function() {
    return this.isHostSet();
  },

  /** @private */
  handleItemClick_: function(event) {
    // We do not open the subpage if the click was on a link.
    if (event.path[0].tagName === 'A') {
      event.stopPropagation();
      return;
    }

    if (!this.isHostSet())
      return;

    settings.navigateTo(settings.routes.MULTIDEVICE_FEATURES);
  },

  /** @private */
  handleButtonClick_: function(event) {
    event.stopPropagation();
    switch (this.pageContentData.mode) {
      case settings.MultiDeviceSettingsMode.NO_HOST_SET:
        this.browserProxy_.showMultiDeviceSetupDialog();
        return;
      case settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER:
      // Intentional fall-through.
      case settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION:
        // If this device is waiting for action on the server or the host
        // device, clicking the button should trigger this action.
        this.browserProxy_.retryPendingHostSetup();
    }
  },

  /** @private */
  openPasswordPromptDialog_: function() {
    this.showPasswordPromptDialog_ = true;
  },

  onDialogClose_: function(event) {
    event.stopPropagation();
    if (event.path.some(element => element.id === 'multidevicePasswordPrompt'))
      this.onPasswordPromptDialogClose_();
  },

  /** @private */
  onPasswordPromptDialogClose_: function() {
    // The password prompt should only be shown when there is a feature waiting
    // to be enabled.
    assert(this.featureToBeEnabledOnceAuthenticated_ !== null);

    // If |this.authToken_| is set when the dialog has been closed, this means
    // that the user entered the correct password into the dialog. Thus, send
    // all pending features to be enabled.
    if (this.authToken_) {
      this.browserProxy_.setFeatureEnabledState(
          this.featureToBeEnabledOnceAuthenticated_, true /* enabled */,
          this.authToken_);

      // Reset |this.authToken_| now that it has been used. This ensures that
      // users cannot keep an old auth token and reuse it on an subsequent
      // request.
      this.authToken_ = '';
    }

    // Either the feature was enabled above or the user canceled the request by
    // clicking "Cancel" on the password dialog. Thus, there is no longer a need
    // to track any pending feature.
    this.featureToBeEnabledOnceAuthenticated_ = null;

    // Remove the password prompt dialog from the DOM.
    this.showPasswordPromptDialog_ = false;
  },

  /**
   * @param {!{detail: !Object}} event
   * @private
   */
  onAuthTokenChanged_: function(event) {
    this.authToken_ = event.detail.value;
  },

  /**
   * Attempt to enable the provided feature. If not authenticated (i.e.,
   * |authToken_| is invalid), display the password prompt to begin the
   * authentication process.
   *
   * @param {!{detail: !Object}} event
   * @private
   */
  onFeatureToggleClicked_: function(event) {
    let feature = event.detail.feature;
    let enabled = event.detail.enabled;

    // Disabling any feature does not require authentication, and enable some
    // features does not require authentication.
    if (!enabled || !this.isAuthenticationRequiredToEnable_(feature)) {
      this.browserProxy_.setFeatureEnabledState(feature, enabled);
      return;
    }

    // If the feature required authentication to be enabled, open the password
    // prompt dialog. This is required every time the user enables a security-
    // sensitive feature (i.e., use of stale auth tokens is not acceptable).
    this.featureToBeEnabledOnceAuthenticated_ = feature;
    this.openPasswordPromptDialog_();
  },

  /**
   * @param {!settings.MultiDeviceFeature} feature The feature to enable.
   * @return {boolean} Whether authentication is required to enable the feature.
   * @private
   */
  isAuthenticationRequiredToEnable_: function(feature) {
    // Enabling SmartLock always requires authentication.
    if (feature == settings.MultiDeviceFeature.SMART_LOCK)
      return true;

    // Enabling any feature besides SmartLock and the Better Together suite does
    // not require authentication.
    if (feature != settings.MultiDeviceFeature.BETTER_TOGETHER_SUITE)
      return false;

    const smartLockState =
        this.getFeatureState(settings.MultiDeviceFeature.SMART_LOCK);

    // If the user is enabling the Better Together suite and this change would
    // result in SmartLock being implicitly enabled, authentication is required.
    // SmartLock is implicitly enabled if it is only currently not enabled due
    // to the suite being disabled or due to the SmartLock host device not
    // having a lock screen set.
    return smartLockState ==
        settings.MultiDeviceFeatureState.UNAVAILABLE_SUITE_DISABLED ||
        smartLockState ==
        settings.MultiDeviceFeatureState.UNAVAILABLE_INSUFFICIENT_SECURITY;
  },

  /** @private */
  onForgetDeviceRequested_: function() {
    this.browserProxy_.removeHostDevice();
    settings.navigateTo(settings.routes.MULTIDEVICE);
  },

  /**
   * Checks if the user is in a nested page without a host set and, if so,
   * navigates them back to the main page.
   * @private
   */
  leaveNestedPageIfNoHostIsSet_: function() {
    // Wait for data to arrive.
    if (!this.pageContentData)
      return;

    // If the user gets to the a nested page without a host (e.g. by clicking a
    // stale 'existing user' notifications after forgetting their host) we
    // direct them back to the main settings page.
    if (settings.routes.MULTIDEVICE != settings.getCurrentRoute() &&
        settings.routes.MULTIDEVICE.contains(settings.getCurrentRoute()) &&
        !this.isHostSet()) {
      settings.navigateTo(settings.routes.MULTIDEVICE);
    }
  },

  /**
   * @param {!MultiDevicePageContentData} newData
   * @private
   */
  onPageContentDataChanged_: function(newData) {
    this.pageContentData = newData;
    this.leaveNestedPageIfNoHostIsSet_();
  },
});
