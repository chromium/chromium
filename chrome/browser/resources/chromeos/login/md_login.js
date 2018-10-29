// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Login UI based on a stripped down OOBE controller.
 */

var isMd = true;

// <include src="md_login_shared.js">
// <include src="login_non_lock_shared.js">
// <include src="notification_card.js">

/**
 * Ensures that the pin keyboard is loaded.
 * @param {object} onLoaded Callback executed when the pin keyboard is loaded.
 */
function ensurePinKeyboardLoaded(onLoaded) {
  'use strict';

  // Wait a frame before running |onLoaded| to avoid any visual glitches.
  setTimeout(onLoaded);
}

cr.define('cr.ui.Oobe', function() {
  return {
    /**
     * Initializes the OOBE flow.  This will cause all C++ handlers to
     * be invoked to do final setup.
     */
    initialize: function() {
      cr.ui.login.DisplayManager.initialize();
      login.WrongHWIDScreen.register();
      login.AccountPickerScreen.register();
      login.GaiaSigninScreen.register();
      login.UserImageScreen.register(/* lazyInit= */ true);
      login.ResetScreen.register();
      login.AutolaunchScreen.register();
      login.KioskEnableScreen.register();
      login.ErrorMessageScreen.register();
      login.TPMErrorMessageScreen.register();
      login.PasswordChangedScreen.register();
      login.TermsOfServiceScreen.register();
      login.SyncConsentScreen.register();
      login.FingerprintSetupScreen.register();
      login.ArcTermsOfServiceScreen.register();
      login.RecommendAppsScreen.register();
      login.AppDownloadingScreen.register();
      login.AppLaunchSplashScreen.register();
      login.ArcKioskSplashScreen.register();
      login.ConfirmPasswordScreen.register();
      login.FatalErrorScreen.register();
      login.DeviceDisabledScreen.register();
      login.UnrecoverableCryptohomeErrorScreen.register();
      login.ActiveDirectoryPasswordChangeScreen.register(/* lazyInit= */ true);
      login.EncryptionMigrationScreen.register();
      login.VoiceInteractionValuePropScreen.register();
      login.WaitForContainerReadyScreen.register();
      login.UpdateRequiredScreen.register();
      login.DiscoverScreen.register();
      login.MarketingOptInScreen.register();
      login.AssistantOptInFlowScreen.register();
      login.MultiDeviceSetupScreen.register();

      cr.ui.Bubble.decorate($('bubble-persistent'));
      $('bubble-persistent').persistent = true;
      $('bubble-persistent').hideOnKeyPress = false;

      cr.ui.Bubble.decorate($('bubble'));
      login.HeaderBar.decorate($('login-header-bar'));
      login.TopHeaderBar.decorate($('top-header-bar'));

      chrome.send('screenStateInitialize');

      if (Oobe.getInstance().showingViewsLogin)
        chrome.send('showAddUser');
    },

    // Dummy Oobe functions not present with stripped login UI.
    initializeA11yMenu: function(e) {},
    handleAccessibilityLinkClick: function(e) {},
    handleSpokenFeedbackClick: function(e) {},
    handleHighContrastClick: function(e) {},
    handleScreenMagnifierClick: function(e) {},
    setUsageStats: function(checked) {},
    setTpmPassword: function(password) {},
    refreshA11yInfo: function(data) {},
    reloadEulaContent: function(data) {},

    /**
     * Reloads content of the page.
     * @param {!Object} data New dictionary with i18n values.
     */
    reloadContent: function(data) {
      loadTimeData.overrideValues(data);
      i18nTemplate.process(document, loadTimeData);
      Oobe.getInstance().updateLocalizedContent_();
    },

    /**
     * Updates "device in tablet mode" state when tablet mode is changed.
     * @param {Boolean} isInTabletMode True when in tablet mode.
     */
    setTabletModeState: function(isInTabletMode) {
      Oobe.getInstance().setTabletModeState_(isInTabletMode);
    },

    /**
     * Updates OOBE configuration when it is loaded.
     * @param {!OobeTypes.OobeConfiguration} configuration OOBE configuration.
     */
    updateOobeConfiguration: function(configuration) {
      Oobe.getInstance().updateOobeConfiguration_(configuration);
    },
  };
});
