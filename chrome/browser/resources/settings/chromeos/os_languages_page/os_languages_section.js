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
      value() {
        const map = new Map();
        if (settings.routes.OS_LANGUAGES_DETAILS) {
          map.set(
              settings.routes.OS_LANGUAGES_DETAILS.path,
              '#languagesSubpageTrigger');
        }
        if (settings.routes.OS_LANGUAGES_SMART_INPUTS) {
          map.set(
              settings.routes.OS_LANGUAGES_SMART_INPUTS.path,
              '#smartInputsSubpageTrigger');
        }
        return map;
      },
    },

    /**
     * This is enabled when language settings update feature flag is enabled.
     * @private
     * */
    languageSettingsV2Enabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('enableLanguageSettingsV2');
      },
    },

    /**
     * This is enabled when any of the smart inputs features is allowed.
     * @private
     * */
    smartInputsEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('allowAssistivePersonalInfo') ||
            loadTimeData.getBoolean('allowEmojiSuggestion');
      },
    }
  },

  /** @private */
  onLanguagesV2Click_() {
    settings.Router.getInstance().navigateTo(
        settings.routes.OS_LANGUAGES_LANGUAGES);
  },

  /** @private */
  onInputClick_() {
    settings.Router.getInstance().navigateTo(
        settings.routes.OS_LANGUAGES_INPUT);
  },

  /** @private */
  onLanguagesTap_() {
    // TODO(crbug.com/950007): Add UMA metric for opening language details.
    settings.Router.getInstance().navigateTo(
        settings.routes.OS_LANGUAGES_DETAILS);
  },

  /** @private */
  onSmartInputsClick_() {
    settings.Router.getInstance().navigateTo(
        settings.routes.OS_LANGUAGES_SMART_INPUTS);
  },

  /**
   * @param {string|undefined} uiLanguage Current UI language fully specified,
   *     e.g. "English (United States)".
   * @param {string} id The input method ID, e.g. "US Keyboard".
   * @param {!Array<!chrome.languageSettingsPrivate.InputMethod>}
   *     enabledInputMethods The list of currently enabled input methods.
   * @param {!LanguageHelper} languageHelper The LanguageHelper object.
   * @return {string} A sublabel for the 'Languages and input' row
   * @private
   */
  getSubLabel_(uiLanguage, id, enabledInputMethods, languageHelper) {
    if (uiLanguage === undefined) {
      return '';
    }
    const languageDisplayName =
        languageHelper.getLanguage(uiLanguage).displayName;
    const inputMethod = enabledInputMethods.find(function(inputMethod) {
      return inputMethod.id === id;
    });
    const inputMethodDisplayName = inputMethod ? inputMethod.displayName : '';
    if (!inputMethodDisplayName) {
      return languageDisplayName;
    }
    // It is OK to use string concatenation here because it is just joining a 2
    // element list (i.e. it's a standard format).
    return languageDisplayName + ', ' + inputMethodDisplayName;
  },
});
