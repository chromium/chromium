// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-edit-dictionary-page' is a sub-page for editing
 * the "dictionary" of custom words used for spell check.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/polymer/v3_0/iron-a11y-keys/iron-a11y-keys.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import '/shared/settings/prefs/prefs.js';
import '../settings_shared.css.js';
import '../settings_vars.css.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import type {IronA11yKeysElement} from 'chrome://resources/polymer/v3_0/iron-a11y-keys/iron-a11y-keys.js';
import {flush, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {GlobalScrollTargetMixin} from '../global_scroll_target_mixin.js';
import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import type {Route} from '../router.js';

import {getTemplate} from './edit_dictionary_page.html.js';
import {LanguagesBrowserProxyImpl} from './languages_browser_proxy.js';

// Max valid word size defined in
// https://cs.chromium.org/chromium/src/components/spellcheck/common/spellcheck_common.h?l=28
const MAX_CUSTOM_DICTIONARY_WORD_BYTES = 99;

export interface SettingsEditDictionaryPageElement {
  $: {
    addWord: CrButtonElement,
    keys: IronA11yKeysElement,
    newWord: CrInputElement,
    noWordsLabel: HTMLElement,
  };
}

const SettingsEditDictionaryPageElementBase =
    GlobalScrollTargetMixin(PolymerElement) as unknown as
    {new (): PolymerElement};

export class SettingsEditDictionaryPageElement extends
    SettingsEditDictionaryPageElementBase {
  static get is() {
    return 'settings-edit-dictionary-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      newWordValue_: {
        type: String,
        value: '',
      },

      /**
       * Needed by GlobalScrollTargetMixin.
       */
      subpageRoute: {
        type: Object,
        value: routes.EDIT_DICTIONARY,
      },

      words_: {
        type: Array,
        value() {
          return [];
        },
      },

      hasWords_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private newWordValue_: string;
  subpageRoute: Route;
  private words_: string[];
  private hasWords_: boolean;
  private languageSettingsPrivate_:
      (typeof chrome.languageSettingsPrivate)|null = null;

  override ready() {
    super.ready();

    this.languageSettingsPrivate_ =
        LanguagesBrowserProxyImpl.getInstance().getLanguageSettingsPrivate();

    this.languageSettingsPrivate_!.getSpellcheckWords().then(words => {
      this.hasWords_ = words.length > 0;
      this.words_ = words;
    });

    this.languageSettingsPrivate_!.onCustomDictionaryChanged.addListener(
        this.onCustomDictionaryChanged_.bind(this));

    // Add a key handler for the new-word input.
    this.$.keys.target = this.$.newWord;
  }

  /**
   * Adds the word in the new-word input to the dictionary.
   */
  private addWordFromInput_() {
    // Spaces are allowed, but removing leading and trailing whitespace.
    const word = this.getTrimmedNewWord_();
    this.newWordValue_ = '';
    if (word) {
      this.languageSettingsPrivate_!.addSpellcheckWord(word);
    }
  }

  /**
   * Check if the field is empty or invalid.
   */
  private disableAddButton_(): boolean {
    return this.getTrimmedNewWord_().length === 0 || this.isWordInvalid_();
  }

  private getErrorMessage_(): string {
    if (this.newWordIsTooLong_()) {
      return loadTimeData.getString('addDictionaryWordLengthError');
    }
    if (this.newWordAlreadyAdded_()) {
      return loadTimeData.getString('addDictionaryWordDuplicateError');
    }
    return '';
  }

  private getTrimmedNewWord_(): string {
    return this.newWordValue_.trim();
  }

  /**
   * If the word is invalid, returns true (or a message if one is provided).
   * Otherwise returns false.
   */
  private isWordInvalid_(): boolean {
    return this.newWordAlreadyAdded_() || this.newWordIsTooLong_();
  }

  private newWordAlreadyAdded_(): boolean {
    return this.words_.includes(this.getTrimmedNewWord_());
  }

  private newWordIsTooLong_(): boolean {
    return this.getTrimmedNewWord_().length > MAX_CUSTOM_DICTIONARY_WORD_BYTES;
  }

  /**
   * Handles tapping on the Add Word button.
   */
  private onAddWordClick_() {
    this.addWordFromInput_();
    this.$.newWord.focus();
  }

  /**
   * Handles updates to the word list. Additions are unshifted to the top
   * of the list so that users can see them easily.
   */
  private onCustomDictionaryChanged_(added: string[], removed: string[]) {
    const wasEmpty = this.words_.length === 0;

    for (const word of removed) {
      const index = this.words_.indexOf(word);
      if (index !== -1) {
        this.splice('words_', index, 1);
      }
    }

    if (this.words_.length === 0 && added.length === 0 && !wasEmpty) {
      this.hasWords_ = false;
    }

    // This is a workaround to ensure the dom-if is set to true before items
    // are rendered so that focus works correctly in Polymer 2; see
    // https://crbug.com/912523.
    if (wasEmpty && added.length > 0) {
      this.hasWords_ = true;
    }

    for (const word of added) {
      if (!this.words_.includes(word)) {
        this.unshift('words_', word);
      }
    }

    // When adding a word to an _empty_ list, the template is expanded. This
    // is a workaround to resize the iron-list as well.
    // TODO(dschuyler): Remove this hack after iron-list no longer needs
    // this workaround to update the list at the same time the template
    // wrapping the list is expanded.
    if (wasEmpty && this.words_.length > 0) {
      flush();
      this.shadowRoot!.querySelector('iron-list')!.notifyResize();
    }
  }

  /**
   * Handles Enter and Escape key presses for the new-word input.
   */
  private onKeysPress_(
      e: CustomEvent<{key: string, keyboardEvent: KeyboardEvent}>) {
    if (e.detail.key === 'enter' && !this.disableAddButton_()) {
      this.addWordFromInput_();
    } else if (e.detail.key === 'esc') {
      (e.detail.keyboardEvent.target as CrInputElement).value = '';
    }
  }

  /**
   * Handles tapping on a "Remove word" icon button.
   */
  private onRemoveWordClick_(e: {model: {item: string}}) {
    this.languageSettingsPrivate_!.removeSpellcheckWord(e.model.item);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-edit-dictionary-page': SettingsEditDictionaryPageElement;
  }
}

customElements.define(
    SettingsEditDictionaryPageElement.is, SettingsEditDictionaryPageElement);
