// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Subpage of settings-multidevice-feature for managing the Smart Lock feature.
 */
cr.exportPath('settings');

cr.define('settings', function() {
  /**
   * The state of the preference controlling Smart Lock's ability to sign-in the
   * user.
   * @enum {string}
   */
  SignInEnabledState = {
    ENABLED: 'enabled',
    DISABLED: 'disabled',
  };

  /** @const {string} */
  SmartLockSignInEnabledPrefName = 'proximity_auth.is_chromeos_login_enabled';

  return {
    SignInEnabledState: SignInEnabledState,
        SmartLockSignInEnabledPrefName: SmartLockSignInEnabledPrefName,
  };
});

Polymer({
  is: 'settings-multidevice-smartlock-subpage',

  behaviors: [
    MultiDeviceFeatureBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /** @type {?SettingsRoutes} */
    routes: {
      type: Object,
      value: settings.routes,
    },

    /**
     * True if Smart Lock is enabled.
     * @private
     */
    smartLockEnabled_: {
      type: Boolean,
      computed: 'computeIsSmartLockEnabled_(pageContentData)',
    },

    /**
     * Whether Smart Lock may be used to sign-in the user (as opposed to only
     * being able to unlock the user's screen).
     * @private {!settings.SignInEnabledState}
     */
    smartLockSignInEnabled_: {
      type: Object,
      value: settings.SignInEnabledState.DISABLED,
    },

    /**
     * True if the user is allowed to enable Smart Lock sign-in.
     * @private
     */
    smartLockSignInAllowed_: {
      type: Boolean,
      value: true,
    },

    /** @private */
    showPasswordPromptDialog_: {
      type: Boolean,
      value: false,
    },
  },

  listeners: {
    'auth-token-changed': 'onAuthTokenChanged_',
    'close': 'onDialogClose_',
  },

  /** @private {?settings.MultiDeviceBrowserProxy} */
  browserProxy_: null,

  /** @override */
  ready: function() {
    this.browserProxy_ = settings.MultiDeviceBrowserProxyImpl.getInstance();

    this.addWebUIListener(
        'smart-lock-signin-enabled-changed',
        this.updateSmartLockSignInEnabled_.bind(this));

    this.addWebUIListener(
        'smart-lock-signin-allowed-changed',
        this.updateSmartLockSignInAllowed_.bind(this));

    this.browserProxy_.getSmartLockSignInEnabled().then(enabled => {
      this.updateSmartLockSignInEnabled_(enabled);
    });

    this.browserProxy_.getSmartLockSignInAllowed().then(allowed => {
      this.updateSmartLockSignInAllowed_(allowed);
    });
  },

  /**
   * Returns true if Smart Lock is an enabled feature.
   * @return {boolean}
   * @private
   */
  computeIsSmartLockEnabled_: function() {
    return !!this.pageContentData &&
        this.getFeatureState(settings.MultiDeviceFeature.SMART_LOCK) ==
        settings.MultiDeviceFeatureState.ENABLED_BY_USER;
  },

  /**
   * Updates the state of the Smart Lock 'sign-in enabled' toggle.
   * @private
   */
  updateSmartLockSignInEnabled_: function(enabled) {
    this.smartLockSignInEnabled_ = enabled ?
        settings.SignInEnabledState.ENABLED :
        settings.SignInEnabledState.DISABLED;
  },

  /**
   * Updates the Smart Lock 'sign-in enabled' toggle such that disallowing
   * sign-in disables the toggle.
   * @private
   */
  updateSmartLockSignInAllowed_: function(allowed) {
    this.smartLockSignInAllowed_ = allowed;
  },

  /** @private */
  openPasswordPromptDialog_: function() {
    this.showPasswordPromptDialog_ = true;
  },

  /**
   * Sets the Smart Lock 'sign-in enabled' pref based on the value of the
   * radio group representing the pref.
   * @private
   */
  onSmartLockSignInEnabledChanged_: function() {
    const radioGroup = this.$$('cr-radio-group');
    const enabled = radioGroup.selected == settings.SignInEnabledState.ENABLED;

    if (!enabled) {
      // No authentication check is required to disable.
      this.browserProxy_.setSmartLockSignInEnabled(false /* enabled */);
      return;
    }

    // Toggle the enabled state back to disabled, as authentication may not
    // succeed. The toggle state updates automatically by the pref listener.
    radioGroup.selected = settings.SignInEnabledState.DISABLED;
    this.openPasswordPromptDialog_();
  },

  /**
   * Completes the transaction of setting the Smart Lock 'sign-in enabled' pref
   * after the user authenticates.
   * @param {!{detail: !Object}} event The event containing the auth token.
   * @private
   */
  onAuthTokenChanged_: function(event) {
    const authToken = event.detail.value;

    // The auth-token-changed event fires after the expiration period (
    // represented by the empty string), so only move forward when the auth
    // token is non-empty.
    if (authToken !== '') {
      this.browserProxy_.setSmartLockSignInEnabled(
          true /* enabled */, authToken);
    }
  },

  /**
   * Updates the state of the password dialog controller flag when the UI
   * element closes.
   * @private
   */
  onDialogClose_: function() {
    this.showPasswordPromptDialog_ = false;
  },
});
