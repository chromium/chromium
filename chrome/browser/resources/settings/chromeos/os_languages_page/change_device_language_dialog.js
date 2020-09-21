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
    I18nBehavior,
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

    /** @private */
    lowercaseQueryString_: {
      type: String,
      value: '',
    },
  },

  /**
   * @param {!CustomEvent<string>} e
   * @private
   */
  onSearchChanged_(e) {
    this.lowercaseQueryString_ = e.detail.toLowerCase();
  },

  /**
   * @return {!Array<!chrome.languageSettingsPrivate.Language>} A list of
   *     possible device language.
   * @private
   */
  getPossibleDeviceLanguages_() {
    return this.languages.supported.filter(language => {
      if (!language.supportsUI || language.isProhibitedLanguage ||
          language.code === this.languages.prospectiveUILanguage) {
        return false;
      }

      return !this.lowercaseQueryString_ ||
          language.displayName.toLowerCase().includes(
              this.lowercaseQueryString_) ||
          language.nativeDisplayName.toLowerCase().includes(
              this.lowercaseQueryString_);
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
   * @param {!chrome.languageSettingsPrivate.Language} item
   * @param {boolean} selected
   * @return {!string}
   * @private
   */
  getAriaLabelForItem_(item, selected) {
    const instruction = selected ? 'selectedDeviceLanguageInstruction' :
                                   'notSelectedDeviceLanguageInstruction';
    return this.i18n(instruction, this.getDisplayText_(item));
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
   * Sets device language and restarts device.
   * @private
   */
  onActionButtonTap_() {
    assert(this.selectedLanguage_);
    const languageCode = this.selectedLanguage_.code;
    this.languageHelper.setProspectiveUILanguage(languageCode);
    // If the language isn't enabled yet, it should be added and moved to top.
    // If it's already present, we don't do anything.
    if (!this.languageHelper.isLanguageEnabled(languageCode)) {
      this.languageHelper.enableLanguage(languageCode);
      this.languageHelper.moveLanguageToFront(languageCode);
    }
    settings.recordSettingChange();
    settings.LanguagesMetricsProxyImpl.getInstance().recordInteraction(
        settings.LanguagesPageInteraction.RESTART);
    settings.LifetimeBrowserProxyImpl.getInstance().signOutAndRestart();
  },

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeydown_(e) {
    // Close dialog if 'esc' is pressed and the search box is already empty.
    if (e.key === 'Escape' && !this.$.search.getValue().trim()) {
      this.$.dialog.close();
    } else if (e.key !== 'PageDown' && e.key !== 'PageUp') {
      this.$.search.scrollIntoViewIfNeeded();
    }
  },
});
