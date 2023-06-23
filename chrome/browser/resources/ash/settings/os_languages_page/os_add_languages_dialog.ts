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

class OsSettingsAddLanguagesDialogElement extends PolymerElement {
  static get is() {
    return 'os-settings-add-languages-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      languages: {
        type: Object,
        notify: true,
      },
      languageHelper: Object,
    };
  }

  // Public API: Downwards data flow.
  languages: LanguagesModel|undefined;
  languageHelper: LanguageHelper;

  /**
   * @return A list of languages to be displayed in the dialog.
   */
  private getLanguages_(): Item[] {
    // This assertion of `this.languages` is potentially unsafe and could fail.
    // TODO(b/265553377): Prove that this assertion is safe, or rewrite this to
    // avoid this assertion.
    return this.languages!.supported
        .filter(language => this.languageHelper.canEnableLanguage(language))
        .map(language => ({
               id: language.code,
               name: this.getDisplayText_(language),
               searchTerms: [language.displayName, language.nativeDisplayName],
               disabledByPolicy: false,
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
   * Enables the checked languages.
   */
  private onItemsAdded_(e: HTMLElementEventMap['items-added']): void {
    e.detail.forEach(languageCode => {
      this.languageHelper.enableLanguage(languageCode);
    });
  }
}

customElements.define(
    OsSettingsAddLanguagesDialogElement.is,
    OsSettingsAddLanguagesDialogElement);

declare global {
  interface HTMLElementTagNameMap {
    [OsSettingsAddLanguagesDialogElement.is]:
        OsSettingsAddLanguagesDialogElement;
  }
}
