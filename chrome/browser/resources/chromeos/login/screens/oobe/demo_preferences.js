// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'demo-preferences-element',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],

  EXTERNAL_API: ['setSelectedKeyboard'],

  properties: {
    /**
     * List of languages for language selector dropdown.
     * @type {!Array<!OobeTypes.LanguageDsc>}
     */
    languages: {
      type: Array,
    },

    /**
     * List of keyboards for keyboard selector dropdown.
     * @type {!Array<!OobeTypes.IMEDsc>}
     */
    keyboards: {
      type: Array,
    },

    /**
     * List of countries for country selector dropdown.
     * @type {!Array<!OobeTypes.DemoCountryDsc>}
     */
    countries: {
      type: Array,
    },

  },

  /**
   * Flag that ensures that OOBE configuration is applied only once.
   * @private {boolean}
   */
  configuration_applied_: false,

  ready() {
    this.initializeLoginScreen('DemoPreferencesScreen', {
      resetAllowed: false,
    });
    this.updateLocalizedContent();
  },

  /** Returns a control which should receive an initial focus. */
  get defaultControl() {
    return this.$.demoPreferencesDialog;
  },

  /** Called when dialog is shown */
  onBeforeShow() {
    window.setTimeout(this.applyOobeConfiguration_.bind(this), 0);
  },

  /** Called when dialog is shown for the first time */
  applyOobeConfiguration_() {
    if (this.configuration_applied_)
      return;
    var configuration = Oobe.getInstance().getOobeConfiguration();
    if (!configuration)
      return;
    if (configuration.demoPreferencesNext) {
      this.onNextClicked_();
    }
    this.configuration_applied_ = true;
  },

  /** Called after resources are updated. */
  updateLocalizedContent() {
    assert(loadTimeData);
    var languageList = /** @type {!Array<OobeTypes.LanguageDsc>} */ (
        loadTimeData.getValue('languageList'));
    this.setLanguageList_(languageList);

    var inputMethodsList = /** @type {!Array<OobeTypes.IMEDsc>} */ (
        loadTimeData.getValue('inputMethodsList'));
    this.setInputMethods_(inputMethodsList);

    var countryList = /** @type {!Array<OobeTypes.DemoCountryDsc>} */ (
        loadTimeData.getValue('demoModeCountryList'));
    this.setCountryList_(countryList);

    this.i18nUpdateLocale();
  },

  /**
   * Sets selected keyboard.
   * @param {string} keyboardId
   */
  setSelectedKeyboard(keyboardId) {
    var found = false;
    for (var keyboard of this.keyboards) {
      if (keyboard.value != keyboardId) {
        keyboard.selected = false;
        continue;
      }
      keyboard.selected = true;
      found = true;
    }
    if (!found)
      return;

    // Force i18n-dropdown to refresh.
    this.keyboards = this.keyboards.slice();
  },

  /**
   * Sets language list.
   * @param {!Array<!OobeTypes.LanguageDsc>} languages
   * @private
   */
  setLanguageList_(languages) {
    this.languages = languages;
  },

  /**
   * Sets input methods.
   * @param {!Array<!OobeTypes.IMEDsc>} inputMethods
   * @private
   */
  setInputMethods_(inputMethods) {
    this.keyboards = inputMethods;
  },

  /**
   * Sets country list.
   * @param {!Array<!OobeTypes.DemoCountryDsc>} countries
   * @private
   */
  setCountryList_(countries) {
    this.countries = countries;
    this.$.countryDropdownContainer.hidden = countries.length == 0;
  },

  /**
   * Handle language selection.
   * @param {!CustomEvent<!OobeTypes.LanguageDsc>} event
   * @private
   */
  onLanguageSelected_(event) {
    var item = event.detail;
    var languageId = item.value;
    chrome.send('DemoPreferencesScreen.setLocaleId', [languageId]);
  },

  /**
   * Handle keyboard layout selection.
   * @param {!CustomEvent<!OobeTypes.IMEDsc>} event
   * @private
   */
  onKeyboardSelected_(event) {
    var item = event.detail;
    var inputMethodId = item.value;
    chrome.send('DemoPreferencesScreen.setInputMethodId', [inputMethodId]);
  },

  /**
   * Handle country selection.
   * @param {!CustomEvent<!OobeTypes.DemoCountryDsc>} event
   * @private
   */
  onCountrySelected_(event) {
    chrome.send(
        'DemoPreferencesScreen.setDemoModeCountry', [event.detail.value]);
  },

  /**
   * Back button click handler.
   * @private
   */
  onBackClicked_() {
    this.userActed('close-setup');
  },

  /**
   * Next button click handler.
   * @private
   */
  onNextClicked_() {
    this.userActed('continue-setup');
  },

});
