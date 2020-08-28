// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-change-device-language-dialog' is a dialog for
 * changing device language.
 */
Polymer({
  is: 'os-settings-change-device-language-dialog',

  behaviors: [
    CrScrollableBehavior,
  ],

  properties: {
    /** @type {!LanguagesModel|undefined} */
    languages: Object,

    /** @type {!LanguageHelper} */
    languageHelper: Object,

    /** @private {?chrome.languageSettingsPrivate.Language} */
    selectedLanguage_: {
      type: Object,
      value: null,
    },

    /** @private */
    disableActionButton_: {
      type: Boolean,
      computed: 'shouldDisableActionButton_(selectedLanguage_)',
    },
  },

  /**
   * @return {!Array<!chrome.languageSettingsPrivate.Language>} A list of
   *     possible device language.
   * @private
   */
  getPossibleDeviceLanguages_() {
    // TODO(crbug/1113439): add search and filter based on search value.
    return this.languages.supported.filter(language => {
      if (!language.supportsUI || language.isProhibitedLanguage ||
          language.code === this.languages.prospectiveUILanguage) {
        return false;
      }

      return true;
    });
  },

  /**
   * @param {boolean} selected
   * @private
   */
  getItemClass_(selected) {
    return selected ? 'selected' : '';
  },

  /**
   * @param {!chrome.languageSettingsPrivate.Language} language
   * @return {string} The text to be displayed.
   * @private
   */
  getDisplayText_(language) {
    let displayText = language.displayName;
    // If the native name is different, add it.
    if (language.displayName != language.nativeDisplayName) {
      displayText += ' - ' + language.nativeDisplayName;
    }
    return displayText;
  },

  /** @private */
  shouldDisableActionButton_() {
    return this.selectedLanguage_ === null;
  },

  /** @private */
  onCancelButtonTap_() {
    this.$.dialog.close();
  },

  /**
   * Sets device language.
   * @private
   */
  onActionButtonTap_() {
    assert(this.selectedLanguage_);
    this.languageHelper.setProspectiveUILanguage(this.selectedLanguage_.code);
    this.$.dialog.close();
  },
});
