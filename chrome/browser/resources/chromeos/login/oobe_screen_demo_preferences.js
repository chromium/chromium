// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Demo mode preferences screen implementation.
 */

login.createScreen('DemoPreferencesScreen', 'demo-preferences', function() {
  var demoPreferencesModule = null;

  return {
    EXTERNAL_API: ['setInputMethodIdFromBackend'],

    /** @override */
    decorate: function() {
      demoPreferencesModule = $('demo-preferences-content');
      demoPreferencesModule.screen = this;

      this.updateLocalizedContent();
    },

    /** Update the current input method. Called from C++. */
    setInputMethodIdFromBackend: function(inputMethodId) {
      $('demo-preferences-content').setSelectedKeyboard(inputMethodId);
    },

    /** Returns a control which should receive an initial focus. */
    get defaultControl() {
      return demoPreferencesModule;
    },

    /** Called after resources are updated. */
    updateLocalizedContent: function() {
      demoPreferencesModule.updateLocalizedContent();
    },

    /**
     * Called when language was selected.
     * @param {string} languageId Id of the selected language.
     */
    onLanguageSelected_: function(languageId) {
      chrome.send('DemoPreferencesScreen.setLocaleId', [languageId]);
    },

    /**
     * Called when keyboard was selected.
     * @param {string} inputMethodId Id of the selected input method.
     */
    onKeyboardSelected_: function(inputMethodId) {
      chrome.send('DemoPreferencesScreen.setInputMethodId', [inputMethodId]);
    },

    /**
     * Called when country was selected.
     * @param {string} countryId Id of the selected country.
     */
    onCountrySelected_: function(countryId) {
      chrome.send('DemoPreferencesScreen.setDemoModeCountry', [countryId]);
    },
  };
});
