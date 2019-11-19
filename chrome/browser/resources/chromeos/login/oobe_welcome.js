// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design OOBE.
 */

Polymer({
  is: 'oobe-welcome-md',

  behaviors: [I18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],

  properties: {
    /**
     * Currently selected system language (display name).
     */
    currentLanguage: {
      type: String,
      value: '',
    },

    /**
     * Currently selected input method (display name).
     */
    currentKeyboard: {
      type: String,
      value: '',
    },

    /**
     * List of languages for language selector dropdown.
     * @type {!Array<OobeTypes.LanguageDsc>}
     */
    languages: {
      type: Array,
      observer: 'onLanguagesChanged_',
    },

    /**
     * List of keyboards for keyboard selector dropdown.
     * @type {!Array<OobeTypes.IMEDsc>}
     */
    keyboards: {
      type: Array,
      observer: 'onKeyboardsChanged_',
    },

    /**
     * Accessibility options status.
     * @type {!OobeTypes.A11yStatuses}
     */
    a11yStatus: {
      type: Object,
    },

    /**
     * A list of timezones for Timezone Selection screen.
     * @type {!Array<OobeTypes.TimezoneDsc>}
     */
    timezones: {
      type: Object,
      value: [],
    },

    /**
     * If UI uses forced keyboard navigation.
     */
    highlightStrength: {
      type: String,
      value: '',
    },

    /**
     * Controls displaying of "Enable debugging features" link.
     */
    debuggingLinkVisible_: Boolean,
  },

  /** Overridden from LoginScreenBehavior. */
  EXTERNAL_API: [
    'onInputMethodIdSetFromBackend',
  ],

  /**
   * Flag that ensures that OOBE configuration is applied only once.
   * @private {boolean}
   */
  configuration_applied_: false,

  /** @override */
  ready: function() {
    this.initializeLoginScreen('WelcomeScreen', {
      resetAllowed: true,
      enableDebuggingAllowed: true,
      enterDemoModeAllowed: true,
      noAnimatedTransition: true,
      postponeEnrollmentAllowed: true,
    });
    this.updateLocalizedContent();
  },

  /**
   * Event handler that is invoked just before the screen is shown.
   * TODO (https://crbug.com/948932): Define this type.
   * @param {Object} data Screen init payload.
   */
  onBeforeShow: function(data) {
    this.behaviors.forEach((behavior) => {
      if (behavior.onBeforeShow)
        behavior.onBeforeShow.call(this);
    });

    this.debuggingLinkVisible_ =
        data && 'isDeveloperMode' in data && data['isDeveloperMode'];

    if (this.fullScreenDialog)
      this.$.welcomeScreen.fullScreenDialog = true;

    this.$.welcomeScreen.onBeforeShow();
    let dialogs = Polymer.dom(this.root).querySelectorAll('oobe-dialog');
    for (let dialog of dialogs)
      dialog.onBeforeShow();

    let activeScreen = this.getActiveScreen_();
    if (activeScreen.show)
      activeScreen.show();

    window.setTimeout(this.applyOobeConfiguration_.bind(this), 0);
  },

  /**
   * This is called when UI strings are changed.
   * Overridden from LoginScreenBehavior.
   */
  updateLocalizedContent: function() {
    this.languages = /** @type {!Array<OobeTypes.LanguageDsc>} */ (
        loadTimeData.getValue('languageList'));
    this.keyboards = /** @type {!Array<OobeTypes.IMEDsc>} */ (
        loadTimeData.getValue('inputMethodsList'));
    this.timezones = /** @type {!Array<OobeTypes.TimezoneDsc>} */ (
        loadTimeData.getValue('timezoneList'));
    this.highlightStrength =
        /** @type {string} */ (loadTimeData.getValue('highlightStrength'));

    this.$.welcomeScreen.i18nUpdateLocale();
    this.i18nUpdateLocale();

    var currentLanguage = loadTimeData.getString('language');

    // We might have changed language via configuration. In this case
    // we need to proceed with rest of configuration after language change
    // was fully resolved.
    var configuration = Oobe.getInstance().getOobeConfiguration();
    if (configuration && configuration.language &&
        configuration.language == currentLanguage) {
      window.setTimeout(this.applyOobeConfiguration_.bind(this), 0);
    }
  },

  /**
   * Called when OOBE configuration is loaded.
   * Overridden from LoginScreenBehavior.
   * @param {!OobeTypes.OobeConfiguration} configuration
   */
  updateOobeConfiguration: function(configuration) {
    if (!this.configuration_applied_)
      window.setTimeout(this.applyOobeConfiguration_.bind(this), 0);
  },

  /**
   * Called when dialog is shown for the first time.
   * @private
   */
  applyOobeConfiguration_: function() {
    if (this.configuration_applied_)
      return;
    var configuration = Oobe.getInstance().getOobeConfiguration();
    if (!configuration)
      return;

    if (configuration.language) {
      var currentLanguage = loadTimeData.getString('language');
      if (currentLanguage != configuration.language) {
        this.applySelectedLanguage_(configuration.language);
        // Trigger language change without marking it as applied.
        // applyOobeConfiguration will be called again once language change
        // was applied.
        return;
      }
    }
    if (configuration.inputMethod)
      this.applySelectedLkeyboard_(configuration.inputMethod);

    if (configuration.welcomeNext)
      this.onWelcomeNextButtonClicked_();

    if (configuration.enableDemoMode)
      Oobe.getInstance().startDemoModeFlow();

    this.configuration_applied_ = true;
  },

  /**
   * Updates "device in tablet mode" state when tablet mode is changed.
   * Overridden from LoginScreenBehavior.
   * @param {boolean} isInTabletMode True when in tablet mode.
   */
  setTabletModeState: function(isInTabletMode) {
    this.$.welcomeScreen.isInTabletMode = isInTabletMode;
  },

  /**
   * Window-resize event listener (delivered through the display_manager).
   */
  onWindowResize: function() {
    this.$.welcomeScreen.onWindowResize();
  },

  /**
   * Hides all screens to help switching from one screen to another.
   * @private
   */
  hideAllScreens_: function() {
    this.$.welcomeScreen.hidden = true;

    var screens = Polymer.dom(this.root).querySelectorAll('oobe-dialog');
    for (var i = 0; i < screens.length; ++i) {
      screens[i].hidden = true;
    }
  },

  /**
   * Shows given screen.
   * @param id String Screen ID.
   * @private
   */
  showScreen_: function(id) {
    this.hideAllScreens_();

    var screen = this.$[id];
    assert(screen);
    screen.hidden = false;
    screen.show();
  },

  /**
   * Returns active screen object.
   * @private
   */
  getActiveScreen_: function() {
    var screens = Polymer.dom(this.root).querySelectorAll('oobe-dialog');
    for (var i = 0; i < screens.length; ++i) {
      if (!screens[i].hidden)
        return screens[i];
    }
    return this.$.welcomeScreen;
  },

  focus: function() {
    this.getActiveScreen_().focus();
  },

  /**
   * Handles "visible" event.
   * @private
   */
  onAnimationFinish_: function() {
    this.focus();
  },

  /**
   * Returns true if timezone button should be visible.
   * @private
   */
  isTimezoneButtonVisible_: function(highlightStrength) {
    return highlightStrength === 'strong';
  },

  /**
   * Handle "Next" button for "Welcome" screen.
   *
   * @private
   */
  onWelcomeNextButtonClicked_: function() {
    chrome.send('login.WelcomeScreen.userActed', ['continue']);
  },

  /**
   * Handles "enable-debugging" link for "Welcome" screen.
   *
   * @private
   */
  onEnableDebuggingClicked_: function() {
    cr.ui.Oobe.handleAccelerator(ACCELERATOR_ENABLE_DEBBUGING);
  },

  /**
   * Handle "launch-advanced-options" button for "Welcome" screen.
   *
   * @private
   */
  onWelcomeLaunchAdvancedOptions_: function() {
    this.showScreen_('oobeAdvancedOptionsScreen');
  },

  /**
   * Handle "Language" button for "Welcome" screen.
   *
   * @private
   */
  onWelcomeSelectLanguageButtonClicked_: function() {
    this.showScreen_('languageScreen');
  },

  /**
   * Handle "Accessibility" button for "Welcome" screen.
   *
   * @private
   */
  onWelcomeAccessibilityButtonClicked_: function() {
    this.showScreen_('accessibilityScreen');
  },

  /**
   * Handle "Timezone" button for "Welcome" screen.
   *
   * @private
   */
  onWelcomeTimezoneButtonClicked_: function() {
    this.showScreen_('timezoneScreen');
  },

  /**
   * Handle language selection.
   *
   * @param {!CustomEvent<!OobeTypes.LanguageDsc>} event
   * @private
   */
  onLanguageSelected_: function(event) {
    var item = event.detail;
    var languageId = item.value;
    this.currentLanguage = item.title;
    this.applySelectedLanguage_(languageId);
  },

  /**
   * Switch UI language.
   *
   * @param {string} languageId
   * @private
   */
  applySelectedLanguage_: function(languageId) {
    chrome.send('WelcomeScreen.setLocaleId', [languageId]);
  },

  /**
   * Handle keyboard layout selection.
   *
   * @param {!CustomEvent<!OobeTypes.IMEDsc>} event
   * @private
   */
  onKeyboardSelected_: function(event) {
    var item = event.detail;
    var inputMethodId = item.value;
    this.currentKeyboard = item.title;
    this.applySelectedLkeyboard_(inputMethodId);
  },

  /**
   * Switch keyboard layout.
   *
   * @param {string} inputMethodId
   * @private
   */
  applySelectedLkeyboard_: function(inputMethodId) {
    chrome.send('WelcomeScreen.setInputMethodId', [inputMethodId]);
  },

  onLanguagesChanged_: function() {
    this.currentLanguage =
        getSelectedTitle(/** @type {!SelectListType} */ (this.languages));
  },

  onInputMethodIdSetFromBackend: function(keyboard_id) {
    var found = false;
    for (var i = 0; i < this.keyboards.length; ++i) {
      if (this.keyboards[i].value != keyboard_id) {
        this.keyboards[i].selected = false;
        continue;
      }
      this.keyboards[i].selected = true;
      found = true;
    }
    if (!found)
      return;

    // Force i18n-dropdown to refresh.
    this.keyboards = this.keyboards.slice();
    this.onKeyboardsChanged_();
  },

  onKeyboardsChanged_: function() {
    this.currentKeyboard = getSelectedTitle(this.keyboards);
  },

  /**
   * Handle "OK" button for "LanguageSelection" screen.
   *
   * @private
   */
  closeLanguageSection_: function() {
    this.showScreen_('welcomeScreen');
  },

  /** ******************** Accessibility section ******************* */

  /**
   * Handle "OK" button for "Accessibility Options" screen.
   *
   * @private
   */
  closeAccessibilitySection_: function() {
    this.showScreen_('welcomeScreen');
  },

  /**
   * Handle all accessibility buttons.
   * Note that each <oobe-a11y-option> has chromeMessage attribute
   * containing Chromium callback name.
   *
   * @private
   * @param {!Event} event
   */
  onA11yOptionChanged_: function(event) {
    var a11ytarget = /** @type {{chromeMessage: string, checked: boolean}} */ (
        event.currentTarget);
    chrome.send(a11ytarget.chromeMessage, [a11ytarget.checked]);
  },

  /** ******************** Timezone section ******************* */

  /**
   * Handle "OK" button for "Timezone Selection" screen.
   *
   * @private
   */
  closeTimezoneSection_: function() {
    this.showScreen_('welcomeScreen');
  },

  /**
   * Handle timezone selection.
   *
   * @param {!CustomEvent<!OobeTypes.Timezone>} event
   * @private
   */
  onTimezoneSelected_: function(event) {
    var item = event.detail;
    if (!item)
      return;

    chrome.send('WelcomeScreen.setTimezoneId', [item.value]);
  },

  /** ******************** AdvancedOptions section ******************* */

  /**
   * Handle "OK" button for "AdvancedOptions Selection" screen.
   *
   * @private
   */
  closeAdvancedOptionsSection_: function() {
    this.showScreen_('welcomeScreen');
  },

  /**
   * Handle click on "Set up as CFM device" option.
   *
   * @private
   */
  onCFMBootstrappingClicked_: function() {
    cr.ui.Oobe.handleAccelerator(ACCELERATOR_DEVICE_REQUISITION_REMORA);
  },

  /**
   * Handle click on "Device requisition" option.
   *
   * @private
   */
  onDeviceRequisitionClicked_: function() {
    cr.ui.Oobe.handleAccelerator(ACCELERATOR_DEVICE_REQUISITION);
  },
});
