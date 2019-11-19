// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-languages-section' is the top-level settings section for
 * languages.
 */
Polymer({
  is: 'os-settings-languages-section',

  properties: {
    prefs: Object,

    /** @type {!LanguagesModel|undefined} */
    languages: {
      type: Object,
      notify: true,
    },

    /** @type {!LanguageHelper} */
    languageHelper: Object,

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value: function() {
        const map = new Map();
        if (settings.routes.LANGUAGES_DETAILS) {
          map.set(
              settings.routes.LANGUAGES_DETAILS.path,
              '#languagesSubpageTrigger');
        }
        return map;
      },
    }
  },

  /** @private */
  onLanguagesTap_: function() {
    // TODO(crbug.com/950007): Add UMA metric for opening language details.
    settings.navigateTo(settings.routes.LANGUAGES_DETAILS);
  },

  /**
   * @param {string} uiLanguage Current UI language fully specified, e.g.
   *     "English (United States)".
   * @param {string} id The input method ID, e.g. "US Keyboard".
   * @return {string} A sublabel for the 'Languages and input' row
   * @private
   */
  getSubLabel_: function(uiLanguage, id) {
    const languageDisplayName =
        this.languageHelper.getLanguage(uiLanguage).displayName;
    const inputMethod =
        this.languages.inputMethods.enabled.find(function(inputMethod) {
          return inputMethod.id == id;
        });
    const inputMethodDisplayName = inputMethod ? inputMethod.displayName : '';
    // It is OK to use string concatenation here because it is just joining a 2
    // element list (i.e. it's a standard format).
    return languageDisplayName + ', ' + inputMethodDisplayName;
  },
});
