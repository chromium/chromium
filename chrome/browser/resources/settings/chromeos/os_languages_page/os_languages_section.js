// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The IME ID for the Accessibility Common extension used by Dictation.
/** @type {string} */
const ACCESSIBILITY_COMMON_IME_ID =
    '_ext_ime_egfdjlfmgnehecnclamagfafdccgfndpdictation';

/**
 * @fileoverview
 * 'os-settings-languages-section' is the top-level settings section for
 * languages.
 */
Polymer({
  is: 'os-settings-languages-section',

  behaviors: [
    I18nBehavior,
  ],

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
        if (settings.routes.OS_LANGUAGES_SMART_INPUTS) {
          map.set(
              settings.routes.OS_LANGUAGES_SMART_INPUTS.path,
              '#smartInputsSubpageTrigger');
        }
        return map;
      },
    },

    /** @private */
    inputPageTitle_: {
      type: String,
      value() {
        const isUpdate2 =
            loadTimeData.getBoolean('enableLanguageSettingsV2Update2');
        return this.i18n(isUpdate2 ? 'inputPageTitleV2' : 'inputPageTitle');
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
            loadTimeData.getBoolean('allowEmojiSuggestion') ||
            loadTimeData.getBoolean('allowPredictiveWriting');
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
  onSmartInputsClick_() {
    settings.Router.getInstance().navigateTo(
        settings.routes.OS_LANGUAGES_SMART_INPUTS);
  },

  /**
   * @param {string|undefined} code The language code of the language.
   * @param {!LanguageHelper} languageHelper The LanguageHelper object.
   * @return {string} The display name of the language specified.
   * @private
   */
  getLanguageDisplayName_(code, languageHelper) {
    if (!code) {
      return '';
    }
    const language = languageHelper.getLanguage(code);
    if (!language) {
      return '';
    }
    return language.displayName;
  },

  /**
   * @param {string|undefined} id The input method ID.
   * @param {!LanguageHelper} languageHelper The LanguageHelper object.
   * @return {string} The display name of the input method.
   * @private
   */
  getInputMethodDisplayName_(id, languageHelper) {
    if (id === undefined) {
      return '';
    }

    if (id === ACCESSIBILITY_COMMON_IME_ID) {
      return '';
    }
    // LanguageHelper.getInputMethodDisplayName will throw an error if the ID
    // isn't found, such as when using CrOS on Linux.
    try {
      return languageHelper.getInputMethodDisplayName(id);
    } catch (_) {
      return '';
    }
  },
});
