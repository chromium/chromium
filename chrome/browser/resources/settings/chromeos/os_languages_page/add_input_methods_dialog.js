// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-add-input-methods-dialog' is a dialog for
 * adding input methods.
 */
Polymer({
  is: 'os-settings-add-input-methods-dialog',

  properties: {
    /** @type {!LanguagesModel|undefined} */
    languages: Object,

    /** @type {!LanguageHelper} */
    languageHelper: Object,

    /** @private {!Set<string>} */
    inputMethodsToAdd_: {
      type: Object,
      value() {
        return new Set();
      },
    },

    /** @private {!Array<!chrome.languageSettingsPrivate.InputMethod>} */
    suggestedInputMethods_: {
      type: Array,
      value: [],
      computed:
          'getSuggestedInputMethods_(languages, languages.enabled.*, languages.inputMethods.*)',
    },

    /** @private */
    showSuggestedList_: {
      type: Boolean,
      value: false,
      computed:
          'shouldShowSuggestedList_(suggestedInputMethods_, lowercaseQueryString_)'
    },

    /** @private */
    disableActionButton_: {
      type: Boolean,
      value: true,
      computed: 'shouldDisableActionButton_(inputMethodsToAdd_.size)',
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
    this.lowercaseQueryString_ = e.detail.toLocaleLowerCase();
  },

  /**
   * Get suggested input methods based on user's enabled languages and ARC IMEs
   * @return {!Array<!chrome.languageSettingsPrivate.InputMethod>}
   * @private
   */
  getSuggestedInputMethods_() {
    const languageCodes = [
      ...this.languageHelper.getEnabledLanguageCodes(),
      this.languageHelper.getArcImeLanguageCode()
    ];
    return this.languageHelper.getInputMethodsForLanguages(languageCodes)
        .filter(inputMethod => {
          if (this.languageHelper.isInputMethodEnabled(inputMethod.id)) {
            return false;
          }
          return !inputMethod.isProhibitedByPolicy;
        });
  },

  /**
   * @return {!Array<!chrome.languageSettingsPrivate.InputMethod>} A list of
   *     possible input methods.
   * @private
   */
  getAllInputMethods_() {
    return this.languages.inputMethods.supported.filter(inputMethod => {
      // Don't show input methods which are already enabled.
      if (this.languageHelper.isInputMethodEnabled(inputMethod.id)) {
        return false;
      }
      // Show input methods whose tags match the query.
      return inputMethod.tags.some(
          tag => tag.toLocaleLowerCase().includes(this.lowercaseQueryString_));
    });
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowSuggestedList_() {
    return this.suggestedInputMethods_.length > 0 &&
        !this.lowercaseQueryString_;
  },

  /**
   * True if the user has chosen to add this input method (checked its
   * checkbox).
   * @param {string} id
   * @return {boolean}
   * @private
   */
  willAdd_(id) {
    return this.inputMethodsToAdd_.has(id);
  },

  /**
   * Handler for an input method checkbox.
   * @param {!{model: !{item: chrome.languageSettingsPrivate.InputMethod},
   *           target: !Element}} e
   * @private
   */
  onCheckboxChange_(e) {
    const inputMethodId = e.model.item.id;
    if (e.target.checked) {
      this.inputMethodsToAdd_.add(inputMethodId);
    } else {
      this.inputMethodsToAdd_.delete(inputMethodId);
    }
    // Polymer doesn't notify changes to set size.
    this.notifyPath('inputMethodsToAdd_.size');
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldDisableActionButton_() {
    return !this.inputMethodsToAdd_.size;
  },

  /** @private */
  onCancelButtonTap_() {
    this.$.dialog.close();
  },

  /**
   * Add input methods.
   * @private
   */
  onActionButtonTap_() {
    this.inputMethodsToAdd_.forEach(id => {
      this.languageHelper.addInputMethod(id);
    });
    settings.recordSettingChange();
    this.$.dialog.close();
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
