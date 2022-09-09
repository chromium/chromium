// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-add-languages-dialog' is a dialog for enabling
 * languages.
 */

import './add_items_dialog.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Item} from './add_items_dialog.js';
import {LanguageHelper, LanguagesModel} from './languages_types.js';
import {getTemplate} from './os_add_languages_dialog.html.js';

/** @polymer */
class OsSettingsAddLanguagesDialogElement extends PolymerElement {
  static get is() {
    return 'os-settings-add-languages-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** @type {!LanguagesModel|undefined} */
      languages: {
        type: Object,
        notify: true,
      },

      /** @type {!LanguageHelper} */
      languageHelper: Object,
    };
  }

  /**
   * @return {!Array<!Item>} A list of languages to be displayed in the dialog.
   * @private
   */
  getLanguages_() {
    return this.languages.supported
        .filter(language => this.languageHelper.canEnableLanguage(language))
        .map(language => ({
               id: language.code,
               name: this.getDisplayText_(language),
               searchTerms: [language.displayName, language.nativeDisplayName],
               disabledByPolicy: false,
             }));
  }

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
  }

  /**
   * Enables the checked languages.
   * @param {!CustomEvent<!Set<string>>} e
   * @private
   */
  onItemsAdded_(e) {
    e.detail.forEach(languageCode => {
      this.languageHelper.enableLanguage(languageCode);
    });
  }
}

customElements.define(
    OsSettingsAddLanguagesDialogElement.is,
    OsSettingsAddLanguagesDialogElement);
