// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-add-spellcheck-language-dialog' is a dialog for
 * adding spell check languages.
 */

import './add_items_dialog.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {recordSettingChange} from '../metrics_recorder.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';

import {Item} from './add_items_dialog.js';
import {getTemplate} from './add_spellcheck_languages_dialog.html.js';
import {LanguageHelper, LanguagesModel} from './languages_types.js';

class OsSettingsAddSpellcheckLanguagesDialogElement extends PolymerElement {
  static get is() {
    return 'os-settings-add-spellcheck-languages-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      languages: Object,

      languageHelper: Object,
    };
  }

  // Public API: Downwards data flow.
  languages: LanguagesModel|undefined;
  languageHelper: LanguageHelper;

  /**
   * Get suggested languages based on enabled languages and input methods.
   */
  private getSuggestedLanguageCodes_(): string[] {
    const languageCodes = new Set([
      ...this
          // This assertion of `this.languages` is potentially unsafe and could
          // fail.
          // TODO(b/265553377): Prove that this assertion is safe, or rewrite
          // this to avoid this assertion.
          .languages!
          // Safety: `LanguagesModel.inputMethods` is always defined on CrOS.
          .inputMethods!.enabled.flatMap(
              inputMethod => inputMethod.languageCodes),
      ...this.languageHelper.getEnabledLanguageCodes(),
    ]);
    // Safety: We checked that `this.languages` is defined above.
    return this.languages!.spellCheckOffLanguages
        .map(spellCheckLang => spellCheckLang.language.code)
        .filter(code => languageCodes.has(code));
  }

  /**
   * Get the list of languages used for the "all languages" section, filtering
   * based on the current search query.
   */
  private getAllLanguages_(): Item[] {
    // This assertion of `this.languages` is potentially unsafe and could fail.
    // TODO(b/265553377): Prove that this assertion is safe, or rewrite this to
    // avoid this assertion.
    return this.languages!.spellCheckOffLanguages.map(
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
   * @return The text to be displayed.
   */
  private getDisplayText_(language: chrome.languageSettingsPrivate.Language):
      string {
    let displayText = language.displayName;
    // If the native name is different, add it.
    if (language.displayName !== language.nativeDisplayName) {
      displayText += ' - ' + language.nativeDisplayName;
    }
    return displayText;
  }

  /**
   * Add spell check languages.
   */
  private onItemsAdded_(e: HTMLElementEventMap['items-added']): void {
    e.detail.forEach(code => {
      this.languageHelper.toggleSpellCheck(code, true);
    });
    recordSettingChange(Setting.kAddSpellCheckLanguage);
  }
}

customElements.define(
    OsSettingsAddSpellcheckLanguagesDialogElement.is,
    OsSettingsAddSpellcheckLanguagesDialogElement);

declare global {
  interface HTMLElementTagNameMap {
    [OsSettingsAddSpellcheckLanguagesDialogElement.is]:
        OsSettingsAddSpellcheckLanguagesDialogElement;
  }
}
