// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'demo-preferences-md',

  behaviors: [I18nBehavior, OobeDialogHostBehavior],

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
  },

  /**
   * Flag that ensures that OOBE configuration is applied only once.
   * @private {boolean}
   */
  configuration_applied_: false,

  /** Called when dialog is shown */
  onBeforeShow: function() {
    this.behaviors.forEach((behavior) => {
      if (behavior.onBeforeShow)
        behavior.onBeforeShow.call(this);
    });
    window.setTimeout(this.applyOobeConfiguration_.bind(this), 0);
  },

  /** Called when dialog is shown for the first time */
  applyOobeConfiguration_: function() {
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
  updateLocalizedContent: function() {
    assert(loadTimeData);
    var languageList = loadTimeData.getValue('languageList');
    this.setLanguageList_(languageList);

    var inputMethodsList = loadTimeData.getValue('inputMethodsList');
    this.setInputMethods_(inputMethodsList);

    this.i18nUpdateLocale();
  },

  /**
   * Sets selected keyboard.
   * @param {string} keyboardId
   */
  setSelectedKeyboard: function(keyboardId) {
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
  setLanguageList_: function(languages) {
    this.languages = languages;
  },

  /**
   * Sets input methods.
   * @param {!Array<!OobeTypes.IMEDsc>} inputMethods
   * @private
   */
  setInputMethods_: function(inputMethods) {
    this.keyboards = inputMethods;
  },

  /**
   * Handle language selection.
   * @param {!{detail: {!OobeTypes.LanguageDsc}}} event
   * @private
   */
  onLanguageSelected_: function(event) {
    var item = event.detail;
    var languageId = item.value;
    this.screen.onLanguageSelected_(languageId);
  },

  /**
   * Handle keyboard layout selection.
   * @param {!{detail: {!OobeTypes.IMEDsc}}} event
   * @private
   */
  onKeyboardSelected_: function(event) {
    var item = event.detail;
    var inputMethodId = item.value;
    this.screen.onKeyboardSelected_(inputMethodId);
  },

  /**
   * Back button click handler.
   * @private
   */
  onBackClicked_: function() {
    chrome.send('login.DemoPreferencesScreen.userActed', ['close-setup']);
  },

  /**
   * Next button click handler.
   * @private
   */
  onNextClicked_: function() {
    chrome.send('login.DemoPreferencesScreen.userActed', ['continue-setup']);
  },

});
