// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'settings-multidevice-smartlock-subpage',

  behaviors: [
    DeepLinkingBehavior,
    MultiDeviceFeatureBehavior,
    settings.RouteObserverBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /** @type {?OsSettingsRoutes} */
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
     * @private {!settings.SmartLockSignInEnabledState}
     */
    smartLockSignInEnabled_: {
      type: Object,
      value: settings.SmartLockSignInEnabledState.DISABLED,
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

    /**
     * Authentication token provided by password-prompt-dialog.
     * @private {!chrome.quickUnlockPrivate.TokenInfo|undefined}
     */
    authToken_: {
      type: Object,
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kSmartLockOnOff,
        chromeos.settings.mojom.Setting.kSmartLockUnlockOrSignIn,
      ]),
    },
  },

  /** @private {?settings.MultiDeviceBrowserProxy} */
  browserProxy_: null,

  /** @override */
  ready() {
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
   * @param {!settings.Route} route
   * @param {!settings.Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== settings.routes.SMART_LOCK) {
      return;
    }

    this.attemptDeepLink();
  },

  /**
   * Returns true if Smart Lock is an enabled feature.
   * @return {boolean}
   * @private
   */
  computeIsSmartLockEnabled_() {
    return !!this.pageContentData &&
        this.getFeatureState(settings.MultiDeviceFeature.SMART_LOCK) ===
        settings.MultiDeviceFeatureState.ENABLED_BY_USER;
  },

  /**
   * Updates the state of the Smart Lock 'sign-in enabled' toggle.
   * @private
   */
  updateSmartLockSignInEnabled_(enabled) {
    this.smartLockSignInEnabled_ =
        enabled ? settings.SmartLockSignInEnabledState.ENABLED :
        settings.SmartLockSignInEnabledState.DISABLED;
  },

  /**
   * Updates the Smart Lock 'sign-in enabled' toggle such that disallowing
   * sign-in disables the toggle.
   * @private
   */
  updateSmartLockSignInAllowed_(allowed) {
    this.smartLockSignInAllowed_ = allowed;
  },

  /** @private */
  openPasswordPromptDialog_() {
    this.showPasswordPromptDialog_ = true;
  },

  /**
   * Sets the Smart Lock 'sign-in enabled' pref based on the value of the
   * radio group representing the pref.
   * @private
   */
  onSmartLockSignInEnabledChanged_() {
    const radioGroup = this.$$('cr-radio-group');
    const enabled =
        radioGroup.selected === settings.SmartLockSignInEnabledState.ENABLED;

    if (!enabled) {
      // No authentication check is required to disable.
      this.browserProxy_.setSmartLockSignInEnabled(false /* enabled */);
      settings.recordSettingChange();
      return;
    }

    // Toggle the enabled state back to disabled, as authentication may not
    // succeed. The toggle state updates automatically by the pref listener.
    radioGroup.selected = settings.SmartLockSignInEnabledState.DISABLED;
    this.openPasswordPromptDialog_();
  },

  /**
   * Updates the state of the password dialog controller flag when the UI
   * element closes.
   * @private
   */
  onEnableSignInDialogClose_() {
    this.showPasswordPromptDialog_ = false;

    // If |this.authToken_| is set when the dialog has been closed, this means
    // that the user entered the correct password into the dialog when
    // attempting to enable SignIn with Smart Lock.
    if (this.authToken_) {
      this.browserProxy_.setSmartLockSignInEnabled(
          true /* enabled */, this.authToken_.token);
      settings.recordSettingChange();
    }

    // Always require password entry if re-enabling SignIn with Smart Lock.
    this.authToken_ = undefined;
  },

  /**
   * @param {!CustomEvent<!chrome.quickUnlockPrivate.TokenInfo>} e
   * @private
   */
  onTokenObtained_(e) {
    this.authToken_ = e.detail;
  },

});
