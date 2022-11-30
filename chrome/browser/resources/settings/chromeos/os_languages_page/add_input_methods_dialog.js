// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-add-input-methods-dialog' is a dialog for
 * adding input methods.
 */

import './add_items_dialog.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {recordSettingChange} from '../metrics_recorder.js';

import {getTemplate} from './add_input_methods_dialog.html.js';
import {Item} from './add_items_dialog.js';
import {LanguageHelper, LanguagesModel} from './languages_types.js';

// The IME ID for the Accessibility Common extension used by Dictation.
/** @type {string} */
const ACCESSIBILITY_COMMON_IME_ID =
    '_ext_ime_egfdjlfmgnehecnclamagfafdccgfndpdictation';

/** @polymer */
class OsSettingsAddInputMethodsDialogElement extends PolymerElement {
  static get is() {
    return 'os-settings-add-input-methods-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** @type {!LanguagesModel|undefined} */
      languages: Object,

      /** @type {!LanguageHelper} */
      languageHelper: Object,
    };
  }

  /**
   * Get suggested input methods based on user's enabled languages and ARC IMEs
   * @return {!Array<!chrome.languageSettingsPrivate.InputMethod>}
   * @private
   */
  getSuggestedInputMethodIds_() {
    const languageCodes = [
      ...this.languageHelper.getEnabledLanguageCodes(),
      this.languageHelper.getArcImeLanguageCode(),
    ];
    let inputMethods =
        this.languageHelper.getInputMethodsForLanguages(languageCodes);
    // Temporary solution for b/237492047: move Vietnamese extension input
    // methods to the top of the suggested list.
    // TODO(b/237492047): Remove this once 1P Vietnamese input methods are
    // suitable for widespread use.
    const isVietnameseExtension = inputMethod =>
        (inputMethod.id.startsWith('_ext_ime_') &&
         inputMethod.languageCodes.includes('vi'));
    inputMethods = inputMethods.filter(isVietnameseExtension)
                       .concat(inputMethods.filter(
                           inputMethod => !isVietnameseExtension(inputMethod)));
    return inputMethods.map(inputMethod => inputMethod.id);
  }

  /**
   * @return {!Array<!Item>} A list of possible input methods.
   * @private
   */
  getInputMethods_() {
    return this.languages.inputMethods.supported
        .filter(inputMethod => {
          // Don't show input methods which are already enabled.
          if (this.languageHelper.isInputMethodEnabled(inputMethod.id)) {
            return false;
          }
          // Don't show the Dictation (Accessibility Common) extension in this
          // list.
          if (inputMethod.id === ACCESSIBILITY_COMMON_IME_ID) {
            return false;
          }
          return true;
        })
        .map(inputMethod => ({
               id: inputMethod.id,
               name: inputMethod.displayName,
               searchTerms: inputMethod.tags,
               disabledByPolicy: !!inputMethod.isProhibitedByPolicy,
             }));
  }

  /**
   * Add input methods.
   * @param {!CustomEvent<!Set<string>>} e
   * @private
   */
  onItemsAdded_(e) {
    e.detail.forEach(id => {
      this.languageHelper.addInputMethod(id);
    });
    recordSettingChange();
  }
}

customElements.define(
    OsSettingsAddInputMethodsDialogElement.is,
    OsSettingsAddInputMethodsDialogElement);
