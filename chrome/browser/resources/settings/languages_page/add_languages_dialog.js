// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-add-languages-dialog' is a dialog for enabling
 * languages.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_search_field/cr_search_field.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import './languages.m.js';
import '../settings_shared_css.m.js';

import {CrScrollableBehavior} from 'chrome://resources/cr_elements/cr_scrollable_behavior.m.js';
import {FindShortcutBehavior} from 'chrome://resources/cr_elements/find_shortcut_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  is: 'settings-add-languages-dialog',

  _template: html`{__html_template__}`,

  behaviors: [
    CrScrollableBehavior,
    FindShortcutBehavior,
  ],

  properties: {
    /** @type {!LanguagesModel|undefined} */
    languages: {
      type: Object,
      notify: true,
    },

    /** @type {!LanguageHelper} */
    languageHelper: Object,

    /** @private {!Set<string>} */
    languagesToAdd_: {
      type: Object,
      value() {
        return new Set();
      },
    },

    /** @private */
    disableActionButton_: {
      type: Boolean,
      value: true,
    },

    /** @private */
    filterValue_: {
      type: String,
      value: '',
    },
  },

  /** @override */
  attached() {
    this.$.dialog.showModal();
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
    this.filterValue_ = e.detail;
  },

  /**
   * @return {!Array<!chrome.languageSettingsPrivate.Language>} A list of
   *     languages to be displayed.
   * @private
   */
  getLanguages_() {
    const filterValue =
        this.filterValue_ ? this.filterValue_.toLowerCase() : null;
    return this.languages.supported.filter(language => {
      if (!this.languageHelper.canEnableLanguage(language)) {
        return false;
      }

      if (filterValue === null) {
        return true;
      }

      return language.displayName.toLowerCase().includes(filterValue) ||
          language.nativeDisplayName.toLowerCase().includes(filterValue);
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
   * True if the user has chosen to add this language (checked its checkbox).
   * @param {string} languageCode
   * @return {boolean}
   * @private
   */
  willAdd_(languageCode) {
    return this.languagesToAdd_.has(languageCode);
  },

  /**
   * Handler for checking or unchecking a language item.
   * @param {!{model: !{item: !chrome.languageSettingsPrivate.Language},
   *           target: !Element}} e
   * @private
   */
  onLanguageCheckboxChange_(e) {
    // Add or remove the item to the Set. No need to worry about data binding:
    // willAdd_ is called to initialize the checkbox state (in case the
    // iron-list re-uses a previous checkbox), and the checkbox can only be
    // changed after that by user action.
    const language = e.model.item;
    if (e.target.checked) {
      this.languagesToAdd_.add(language.code);
    } else {
      this.languagesToAdd_.delete(language.code);
    }

    this.disableActionButton_ = !this.languagesToAdd_.size;
  },

  /** @private */
  onCancelButtonTap_() {
    this.$.dialog.close();
  },

  /**
   * Enables the checked languages.
   * @private
   */
  onActionButtonTap_() {
    this.$.dialog.close();
    this.languagesToAdd_.forEach(languageCode => {
      this.languageHelper.enableLanguage(languageCode);
    });
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
