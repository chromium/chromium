// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Out of the box experience flow (OOBE).
 * This is the main code for the OOBE WebUI implementation.
 */

// <include src="test_util.js">
// <include src="../../../../../ui/login/screen.js">
// <include src="../../../../../ui/login/bubble.js">
// <include src="../../../../../ui/login/display_manager.js">
// <include src="demo_mode_test_helper.js">

// <include
// src="../../../../../ui/login/account_picker/chromeos_screen_account_picker.js">

// <include src="../../../../../ui/login/login_ui_tools.js">
// <include
// src="../../../../../ui/login/account_picker/chromeos_user_pod_row.js">
// <include src="cr_ui.js">
// <include src="oobe_screen_reset.js">
// <include src="oobe_screen_autolaunch.js">
// <include src="oobe_screen_enable_kiosk.js">
// <include src="oobe_screen_supervision_transition.js">
// <include src="oobe_screen_assistant_optin_flow.js">
// <include src="oobe_select.js">

// <include src="screen_app_launch_splash.js">
// <include src="screen_arc_kiosk_splash.js">
// <include src="screen_arc_terms_of_service.js">
// <include src="screen_error_message.js">
// <include src="screen_password_changed.js">
// <include src="screen_tpm_error.js">
// <include src="screen_wrong_hwid.js">
// <include src="screen_confirm_password.js">
// <include src="screen_fatal_error.js">
// <include src="screen_device_disabled.js">
// <include src="screen_active_directory_password_change.js">
// <include src="screen_encryption_migration.js">
// <include src="screen_update_required.js">
// <include src="screen_sync_consent.js">
// <include src="screen_fingerprint_setup.js">
// <include src="screen_recommend_apps.js">
// <include src="screen_app_downloading.js">
// <include src="screen_discover.js">
// <include src="screen_marketing_opt_in.js">
// <include src="screen_multidevice_setup.js">

// <include src="../../gaia_auth_host/authenticator.js">
// <include src="oobe_screen_auto_enrollment_check.js">
// <include src="oobe_screen_demo_setup.js">
// <include src="oobe_screen_demo_preferences.js">
// <include src="oobe_screen_enable_debugging.js">
// <include src="oobe_screen_eula.js">
// <include src="oobe_screen_hid_detection.js">
// <include src="oobe_screen_network.js">
// <include src="oobe_screen_oauth_enrollment.js">
// <include src="oobe_screen_update.js">
// <include src="multi_tap_detector.js">
// <include src="web_view_helper.js">

cr.define('cr.ui.Oobe', function() {
  return {
    /**
     * Initializes the OOBE flow.  This will cause all C++ handlers to
     * be invoked to do final setup.
     */
    initialize: function() {
      cr.ui.login.DisplayManager.initialize();
      login.HIDDetectionScreen.register();
      login.WrongHWIDScreen.register();
      login.NetworkScreen.register();
      login.EulaScreen.register();
      login.UpdateScreen.register();
      login.AutoEnrollmentCheckScreen.register();
      login.EnableDebuggingScreen.register();
      login.ResetScreen.register();
      login.AutolaunchScreen.register();
      login.KioskEnableScreen.register();
      login.AccountPickerScreen.register();
      login.OAuthEnrollmentScreen.register();
      login.ErrorMessageScreen.register();
      login.TPMErrorMessageScreen.register();
      login.PasswordChangedScreen.register();
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
      login.ActiveDirectoryPasswordChangeScreen.register(/* lazyInit= */ true);
      login.SupervisionTransitionScreen.register();
      login.DemoSetupScreen.register();
      login.DemoPreferencesScreen.register();
      login.DiscoverScreen.register();
      login.MarketingOptInScreen.register();
      login.AssistantOptInFlowScreen.register();
      login.MultiDeviceSetupScreen.register();
      login.UpdateRequiredScreen.register();

      cr.ui.Bubble.decorate($('bubble-persistent'));
      $('bubble-persistent').persistent = true;
      $('bubble-persistent').hideOnKeyPress = false;

      cr.ui.Bubble.decorate($('bubble'));

      chrome.send('screenStateInitialize');
    },

    /**
     * Sets usage statistics checkbox.
     * @param {boolean} checked Is the checkbox checked?
     */
    setUsageStats: function(checked) {
      $('oobe-eula-md').usageStatsChecked = checked;
    },

    /**
     * Sets TPM password.
     * @param {text} password TPM password to be shown.
     */
    setTpmPassword: function(password) {
      $('eula').setTpmPassword(password);
    },

    /**
     * Refreshes a11y menu state.
     * @param {!Object} data New dictionary with a11y features state.
     */
    refreshA11yInfo: function(data) {
      $('connect').a11yStatus = data;
    },

    /**
     * Reloads content of the page (localized strings, options of the select
     * controls).
     * @param {!Object} data New dictionary with i18n values.
     */
    reloadContent: function(data) {
      // Reload global local strings, process DOM tree again.
      loadTimeData.overrideValues(data);
      i18nTemplate.process(document, loadTimeData);

      // Update localized content of the screens.
      Oobe.updateLocalizedContent();
    },

    /**
     * Updates "device in tablet mode" state when tablet mode is changed.
     * @param {Boolean} isInTabletMode True when in tablet mode.
     */
    setTabletModeState: function(isInTabletMode) {
      Oobe.getInstance().setTabletModeState_(isInTabletMode);
    },

    /**
     * Reloads localized strings for the eula page.
     * @param {!Object} data New dictionary with changed eula i18n values.
     */
    reloadEulaContent: function(data) {
      loadTimeData.overrideValues(data);
      i18nTemplate.process(document, loadTimeData);
    },

    /**
     * Updates localized content of the screens.
     * Should be executed on language change.
     */
    updateLocalizedContent: function() {
      // Buttons, headers and links.
      Oobe.getInstance().updateLocalizedContent_();
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
