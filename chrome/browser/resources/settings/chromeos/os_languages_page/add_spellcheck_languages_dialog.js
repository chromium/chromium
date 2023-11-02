// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-add-spellcheck-language-dialog' is a dialog for
 * adding spell check languages.
 */

import './add_items_dialog.js';

import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {recordSettingChange} from '../metrics_recorder.js';
import {PrefsBehavior, PrefsBehaviorInterface} from '../prefs_behavior.js';

import {Item} from './add_items_dialog.js';
import {getTemplate} from './add_spellcheck_languages_dialog.html.js';
import {LanguageHelper, LanguagesModel} from './languages_types.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {PrefsBehaviorInterface}
 */
const OsSettingsAddSpellcheckLanguagesDialogElementBase =
    mixinBehaviors([PrefsBehavior], PolymerElement);

/** @polymer */
class OsSettingsAddSpellcheckLanguagesDialogElement extends
    OsSettingsAddSpellcheckLanguagesDialogElementBase {
  static get is() {
    return 'os-settings-add-spellcheck-languages-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /* Preferences state. */
      prefs: {
        type: Object,
        notify: true,
      },

      /** @type {!LanguagesModel|undefined} */
      languages: Object,

      /** @type {!LanguageHelper} */
      languageHelper: Object,
    };
  }

  /**
   * Get suggested languages based on enabled languages and input methods.
   * @return {!Array<string>}
   * @private
   */
  getSuggestedLanguageCodes_() {
    const languageCodes = new Set([
      ...this.languages.inputMethods.enabled.flatMap(
          inputMethod => inputMethod.languageCodes),
      ...this.languageHelper.getEnabledLanguageCodes(),
    ]);
    return this.languages.spellCheckOffLanguages
        .map(spellCheckLang => spellCheckLang.language.code)
        .filter(code => languageCodes.has(code));
  }

  /**
   * Get the list of languages used for the "all languages" section, filtering
   * based on the current search query.
   * @return {!Array<!Item>}
   * @private
   */
  getAllLanguages_() {
    return this.languages.spellCheckOffLanguages.map(
        spellCheckLang => ({
          id: spellCheckLang.language.code,
          name: this.getDisplayText_(spellCheckLang.language),
          searchTerms: [
            spellCheckLang.language.displayName,
            spellCheckLang.language.nativeDisplayName,
          ],
          disabledByPolicy: spellCheckLang.isManaged,
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
   * Add spell check languages.
   * @param {!CustomEvent<!Set<string>>} e
   * @private
   */
  onItemsAdded_(e) {
    e.detail.forEach(code => {
      this.languageHelper.toggleSpellCheck(code, true);
    });
    recordSettingChange();
  }
}

customElements.define(
    OsSettingsAddSpellcheckLanguagesDialogElement.is,
    OsSettingsAddSpellcheckLanguagesDialogElement);
