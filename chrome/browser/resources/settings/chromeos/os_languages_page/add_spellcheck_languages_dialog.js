// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-add-spellcheck-language-dialog' is a dialog for
 * adding spell check languages.
 */
import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_search_field/cr_search_field.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.m.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '//resources/polymer/v3_0/iron-list/iron-list.js';
import './cr_checkbox_with_policy.js';
import './shared_style.js';
import '../../settings_shared_css.js';

import {CrScrollableBehavior} from '//resources/cr_elements/cr_scrollable_behavior.m.js';
import {FindShortcutBehavior} from '//resources/cr_elements/find_shortcut_behavior.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {recordSettingChange} from '../metrics_recorder.js';
import {PrefsBehavior} from '../prefs_behavior.js';

import {LanguageHelper, LanguagesModel, SpellCheckLanguageState} from './languages_types.js';

Polymer({
  is: 'os-settings-add-spellcheck-languages-dialog',

  _template: html`{__html_template__}`,

  behaviors: [
    CrScrollableBehavior,
    PrefsBehavior,
    FindShortcutBehavior,
  ],

  properties: {
    /* Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @type {!LanguagesModel|undefined} */
    languages: Object,

    /** @type {!LanguageHelper} */
    languageHelper: Object,

    /** @private {!Set<string>} */
    languageCodesToAdd_: {
      type: Object,
      value() {
        return new Set();
      },
    },

    /** @private {!Array<!SpellCheckLanguageState>} */
    allLanguages_: {
      type: Array,
      value: [],
      computed: `getAllLanguages_(languages.spellCheckOffLanguages.*,
          lowercaseQuery_)`,
    },

    /** @private */
    languagesExist_: {
      type: Boolean,
      value: false,
      computed: 'computeLanguagesExist_(allLanguages_.length)',
    },

    /** @private {!Array<!SpellCheckLanguageState>} */
    suggestedLanguages_: {
      type: Array,
      value: [],
      computed: `getSuggestedLanguages_(languages.spellCheckOffLanguages.*,
          languages.enabled.*, languages.inputMethods.enabled.*)`,
    },

    /** @private */
    showSuggestedList_: {
      type: Boolean,
      value: false,
      computed: `shouldShowSuggestedList_(suggestedLanguages_, lowercaseQuery_,
          languagesExist_)`,
    },

    /** @private */
    disableActionButton_: {
      type: Boolean,
      value: true,
      computed: 'shouldDisableActionButton_(languageCodesToAdd_.size)',
    },

    /** @private */
    lowercaseQuery_: {
      type: String,
      value: '',
    },
  },

  // Override FindShortcutBehavior methods.
  handleFindShortcut(modalContextOpen) {
    // Assumes this is the only open modal.
    const searchInput = this.$.search.getSearchInput();
    searchInput.scrollIntoViewIfNeeded();
    if (!this.searchInputHasFocus()) {
      searchInput.focus();
    }
    return true;
  },

  // Override FindShortcutBehavior methods.
  searchInputHasFocus() {
    return this.$.search.getSearchInput() ===
        this.$.search.shadowRoot.activeElement;
  },

  /**
   * @param {!CustomEvent<string>} e
   * @private
   */
  onSearchChanged_(e) {
    this.lowercaseQuery_ = e.detail.toLocaleLowerCase();
  },

  /**
   * True if the user has chosen to add this spell check language (checked its
   * checkbox).
   * @param {string} code
   * @return {boolean}
   * @private
   */
  willAdd_(code) {
    return this.languageCodesToAdd_.has(code);
  },

  /**
   * Handler for an input method checkbox.
   * @param {!{model: !{item: SpellCheckLanguageState},
   *           target: !Element}} e
   * @private
   */
  onCheckboxChange_(e) {
    const languageCode = e.model.item.language.code;
    if (e.target.checked) {
      this.languageCodesToAdd_.add(languageCode);
    } else {
      this.languageCodesToAdd_.delete(languageCode);
    }
    // Polymer doesn't notify changes to set size.
    this.notifyPath('languageCodesToAdd_.size');
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowSuggestedList_() {
    return this.suggestedLanguages_.length > 0 && !this.lowercaseQuery_ &&
        this.languagesExist_;
  },

  /**
   * Get suggested languages based on enabled languages and input methods.
   * @return {!Array<!SpellCheckLanguageState>}
   * @private
   */
  getSuggestedLanguages_() {
    const languageCodes = new Set([
      ...this.languages.inputMethods.enabled.flatMap(
          inputMethod => inputMethod.languageCodes),
      ...this.languageHelper.getEnabledLanguageCodes(),
    ]);
    return this.languages.spellCheckOffLanguages.filter(
        spellCheckLanguage =>
            languageCodes.has(spellCheckLanguage.language.code) &&
            !spellCheckLanguage.isManaged);
  },

  /**
   * Get the list of languages used for the "all languages" section, filtering
   * based on the current search query.
   * @return {!Array<!SpellCheckLanguageState>}
   * @private
   */
  getAllLanguages_() {
    return this.languages.spellCheckOffLanguages.filter(langState => {
      const language = langState.language;
      return language.displayName.toLowerCase().includes(
                 this.lowercaseQuery_) ||
          language.nativeDisplayName.toLowerCase().includes(
              this.lowercaseQuery_);
    });
  },

  /**
   * @param {!chrome.languageSettingsPrivate.Language} language
   * @return {string} The text to be displayed.
   * @private
   */
  getDisplayText_(language) {
    let displayText = language.displayName;
    // If the native name is different, add it.
    if (language.displayName !== language.nativeDisplayName) {
      displayText += ' - ' + language.nativeDisplayName;
    }
    return displayText;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldDisableActionButton_() {
    return !this.languageCodesToAdd_.size;
  },

  /** @private */
  onCancelButtonClick_() {
    this.$.dialog.close();
  },

  /**
   * Add spell check languages.
   * @private
   */
  onActionButtonClick_() {
    this.languageCodesToAdd_.forEach(code => {
      this.languageHelper.toggleSpellCheck(code, true);
    });
    recordSettingChange();
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

  /**
   * @return {boolean}
   * @private
   */
  computeLanguagesExist_() {
    return !!this.allLanguages_.length;
  },
});
