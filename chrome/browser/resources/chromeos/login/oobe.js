// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Out of the box experience flow (OOBE).
 * This is the main code for the OOBE WebUI implementation.
 */

// <include src="md_login_shared.js">
// <include src="login_non_lock_shared.js">
// <include src="oobe_screen_auto_enrollment_check.js">
// <include src="oobe_screen_controller_pairing.js">
// <include src="oobe_screen_demo_setup.js">
// <include src="oobe_screen_demo_preferences.js">
// <include src="oobe_screen_enable_debugging.js">
// <include src="oobe_screen_eula.js">
// <include src="oobe_screen_hid_detection.js">
// <include src="oobe_screen_host_pairing.js">
// <include src="oobe_screen_network.js">
// <include src="oobe_screen_update.js">
// <include src="oobe_screen_welcome.js">
// <include src="multi_tap_detector.js">
// <include src="web_view_helper.js">
// <include src="demo_mode_test_helper.js">

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
      login.WelcomeScreen.register();
      login.NetworkScreen.register();
      login.EulaScreen.register();
      login.UpdateScreen.register();
      login.AutoEnrollmentCheckScreen.register();
      login.EnableDebuggingScreen.register();
      login.ResetScreen.register();
      login.AutolaunchScreen.register();
      login.KioskEnableScreen.register();
      login.AccountPickerScreen.register();
      login.GaiaSigninScreen.register();
      login.UserImageScreen.register(/* lazyInit= */ false);
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
      login.ControllerPairingScreen.register();
      login.HostPairingScreen.register();
      login.DeviceDisabledScreen.register();
      login.ActiveDirectoryPasswordChangeScreen.register(/* lazyInit= */ true);
      login.VoiceInteractionValuePropScreen.register();
      login.WaitForContainerReadyScreen.register();
      login.DemoSetupScreen.register();
      login.DemoPreferencesScreen.register();
      login.DiscoverScreen.register();
      login.MarketingOptInScreen.register();
      login.AssistantOptInFlowScreen.register();
      login.MultiDeviceSetupScreen.register();

      cr.ui.Bubble.decorate($('bubble-persistent'));
      $('bubble-persistent').persistent = true;
      $('bubble-persistent').hideOnKeyPress = false;

      cr.ui.Bubble.decorate($('bubble'));
      login.HeaderBar.decorate($('login-header-bar'));
      if ($('top-header-bar'))
        login.TopHeaderBar.decorate($('top-header-bar'));

      Oobe.initializeA11yMenu();

      chrome.send('screenStateInitialize');
    },

    /**
     * Initializes OOBE accessibility menu.
     */
    initializeA11yMenu: function() {
      cr.ui.Bubble.decorate($('accessibility-menu'));
      // Same behaviour on hitting spacebar. See crbug.com/342991.
      function reactOnSpace(event) {
        if (event.keyCode == 32)
          Oobe.handleAccessibilityLinkClick(event);
      }

      $('high-contrast')
          .addEventListener('click', Oobe.handleHighContrastClick);
      $('large-cursor').addEventListener('click', Oobe.handleLargeCursorClick);
      $('spoken-feedback')
          .addEventListener('click', Oobe.handleSpokenFeedbackClick);
      $('select-to-speak')
          .addEventListener('click', Oobe.handleSelectToSpeakClick);
      $('screen-magnifier')
          .addEventListener('click', Oobe.handleScreenMagnifierClick);
      $('virtual-keyboard')
          .addEventListener('click', Oobe.handleVirtualKeyboardClick);

      $('high-contrast').addEventListener('keypress', Oobe.handleA11yKeyPress);
      $('large-cursor').addEventListener('keypress', Oobe.handleA11yKeyPress);
      $('spoken-feedback')
          .addEventListener('keypress', Oobe.handleA11yKeyPress);
      $('select-to-speak')
          .addEventListener('keypress', Oobe.handleA11yKeyPress);
      $('screen-magnifier')
          .addEventListener('keypress', Oobe.handleA11yKeyPress);
      $('virtual-keyboard')
          .addEventListener('keypress', Oobe.handleA11yKeyPress);

      // A11y menu should be accessible i.e. disable autohide on any
      // keydown or click inside menu.
      $('accessibility-menu').hideOnKeyPress = false;
      $('accessibility-menu').hideOnSelfClick = false;
    },

    /**
     * Accessibility link handler.
     */
    handleAccessibilityLinkClick: function(e) {
      /** @const */ var BUBBLE_OFFSET = 5;
      /** @const */ var BUBBLE_PADDING = 10;
      $('accessibility-menu')
          .showForElement(
              e.target, cr.ui.Bubble.Attachment.BOTTOM, BUBBLE_OFFSET,
              BUBBLE_PADDING);

      var maxHeight = cr.ui.LoginUITools.getMaxHeightBeforeShelfOverlapping(
          $('accessibility-menu'));
      if (maxHeight < $('accessibility-menu').offsetHeight) {
        $('accessibility-menu')
            .showForElement(
                e.target, cr.ui.Bubble.Attachment.TOP, BUBBLE_OFFSET,
                BUBBLE_PADDING);
      }

      $('accessibility-menu').firstBubbleElement = $('spoken-feedback');
      $('accessibility-menu').lastBubbleElement = $('close-accessibility-menu');
      $('spoken-feedback').focus();

      if (Oobe.getInstance().currentScreen &&
          Oobe.getInstance().currentScreen.defaultControl) {
        $('accessibility-menu').elementToFocusOnHide =
            Oobe.getInstance().currentScreen.defaultControl;
      } else {
        // Update screen falls into this category. Since it doesn't have any
        // controls other than a11y link we don't want that link to receive
        // focus when screen is shown i.e. defaultControl is not defined.
        // Focus a11y link instead.
        $('accessibility-menu').elementToFocusOnHide = e.target;
      }
      e.stopPropagation();
    },

    /**
     * handle a11y menu checkboxes keypress event by simulating click event.
     */
    handleA11yKeyPress: function(e) {
      if (e.key != 'Enter')
        return;

      if (e.target.tagName != 'INPUT' || e.target.type != 'checkbox')
        return;

      // Simulate click on the checkbox.
      e.target.click();
    },

    /**
     * Spoken feedback checkbox handler.
     */
    handleSpokenFeedbackClick: function(e) {
      chrome.send('enableSpokenFeedback', [$('spoken-feedback').checked]);
      e.stopPropagation();
    },

    /**
     * Select to speak checkbox handler.
     */
    handleSelectToSpeakClick: function(e) {
      chrome.send('enableSelectToSpeak', [$('select-to-speak').checked]);
      e.stopPropagation();
    },

    /**
     * Large cursor checkbox handler.
     */
    handleLargeCursorClick: function(e) {
      chrome.send('enableLargeCursor', [$('large-cursor').checked]);
      e.stopPropagation();
    },

    /**
     * High contrast mode checkbox handler.
     */
    handleHighContrastClick: function(e) {
      chrome.send('enableHighContrast', [$('high-contrast').checked]);
      e.stopPropagation();
    },

    /**
     * Screen magnifier checkbox handler.
     */
    handleScreenMagnifierClick: function(e) {
      chrome.send('enableScreenMagnifier', [$('screen-magnifier').checked]);
      e.stopPropagation();
    },

    /**
     * On-screen keyboard checkbox handler.
     */
    handleVirtualKeyboardClick: function(e) {
      chrome.send('enableVirtualKeyboard', [$('virtual-keyboard').checked]);
      e.stopPropagation();
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
      $('high-contrast').checked = data.highContrastEnabled;
      $('spoken-feedback').checked = data.spokenFeedbackEnabled;
      $('select-to-speak').checked = data.selectToSpeakEnabled;
      $('screen-magnifier').checked = data.screenMagnifierEnabled;
      $('docked-magnifier').checked = data.dockedMagnifierEnabled;
      $('large-cursor').checked = data.largeCursorEnabled;
      $('virtual-keyboard').checked = data.virtualKeyboardEnabled;

      // TODO(katie): Remove this when launching features in OOBE screen.
      if (!data.enableExperimentalA11yFeatures)
        $('docked-magnifier-row').setAttribute('hidden', true);

      $('oobe-welcome-md').a11yStatus = data;
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
