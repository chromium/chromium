// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-edit-dictionary-page' is a sub-page for editing
 * the "dictionary" of custom words used for spell check.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-a11y-keys/iron-a11y-keys.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import '../prefs/prefs.js';
import '../settings_shared_css.js';
import '../settings_vars_css.js';

import {flush, html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {GlobalScrollTargetBehavior} from '../global_scroll_target_behavior.js';
import {loadTimeData} from '../i18n_setup.js';
import {PrefsBehavior} from '../prefs/prefs_behavior.js';
import {routes} from '../route.js';

import {LanguagesBrowserProxyImpl} from './languages_browser_proxy.js';

// Max valid word size defined in
// https://cs.chromium.org/chromium/src/components/spellcheck/common/spellcheck_common.h?l=28
const MAX_CUSTOM_DICTIONARY_WORD_BYTES = 99;

Polymer({
  is: 'settings-edit-dictionary-page',

  _template: html`{__html_template__}`,

  behaviors: [GlobalScrollTargetBehavior],

  properties: {
    /** @private {string} */
    newWordValue_: {
      type: String,
      value: '',
    },

    /**
     * Needed by GlobalScrollTargetBehavior.
     * @override
     */
    subpageRoute: {
      type: Object,
      value: routes.EDIT_DICTIONARY,
    },

    /** @private {!Array<string>} */
    words_: {
      type: Array,
      value() {
        return [];
      },
    },

    /** @private {boolean} */
    hasWords_: {
      type: Boolean,
      value: false,
    },
  },

  /** @private {LanguageSettingsPrivate} */
  languageSettingsPrivate_: null,

  /** @override */
  ready() {
    this.languageSettingsPrivate_ =
        LanguagesBrowserProxyImpl.getInstance().getLanguageSettingsPrivate();

    this.languageSettingsPrivate_.getSpellcheckWords(words => {
      this.hasWords_ = words.length > 0;
      this.words_ = words;
    });

    this.languageSettingsPrivate_.onCustomDictionaryChanged.addListener(
        this.onCustomDictionaryChanged_.bind(this));

    // Add a key handler for the new-word input.
    this.$.keys.target = this.$.newWord;
  },

  /**
   * Adds the word in the new-word input to the dictionary.
   * @private
   */
  addWordFromInput_() {
    // Spaces are allowed, but removing leading and trailing whitespace.
    const word = this.getTrimmedNewWord_();
    this.newWordValue_ = '';
    if (word) {
      this.languageSettingsPrivate_.addSpellcheckWord(word);
    }
  },

  /**
   * Check if the field is empty or invalid.
   * @return {boolean}
   * @private
   */
  disableAddButton_() {
    return this.getTrimmedNewWord_().length === 0 || this.isWordInvalid_();
  },

  /**
   * @return {string}
   * @private
   */
  getErrorMessage_() {
    if (this.newWordIsTooLong_()) {
      return loadTimeData.getString('addDictionaryWordLengthError');
    }
    if (this.newWordAlreadyAdded_()) {
      return loadTimeData.getString('addDictionaryWordDuplicateError');
    }
    return '';
  },

  /**
   * @return {string}
   * @private
   */
  getTrimmedNewWord_() {
    return this.newWordValue_.trim();
  },

  /**
   * If the word is invalid, returns true (or a message if one is provided).
   * Otherwise returns false.
   * @return {boolean}
   * @private
   */
  isWordInvalid_() {
    return this.newWordAlreadyAdded_() || this.newWordIsTooLong_();
  },

  /**
   * @return {boolean}
   * @private
   */
  newWordAlreadyAdded_() {
    return this.words_.includes(this.getTrimmedNewWord_());
  },

  /**
   * @return {boolean}
   * @private
   */
  newWordIsTooLong_() {
    return this.getTrimmedNewWord_().length > MAX_CUSTOM_DICTIONARY_WORD_BYTES;
  },

  /**
   * Handles tapping on the Add Word button.
   */
  onAddWordTap_(e) {
    this.addWordFromInput_();
    this.$.newWord.focus();
  },

  /**
   * Handles updates to the word list. Additions are unshifted to the top
   * of the list so that users can see them easily.
   * @param {!Array<string>} added
   * @param {!Array<string>} removed
   */
  onCustomDictionaryChanged_(added, removed) {
    const wasEmpty = this.words_.length === 0;

    for (const word of removed) {
      this.arrayDelete('words_', word);
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
      this.$$('#list').notifyResize();
    }
  },

  /**
   * Handles Enter and Escape key presses for the new-word input.
   * @param {!CustomEvent<!{key: string}>} e
   */
  onKeysPress_(e) {
    if (e.detail.key === 'enter' && !this.disableAddButton_()) {
      this.addWordFromInput_();
    } else if (e.detail.key === 'esc') {
      e.detail.keyboardEvent.target.value = '';
    }
  },

  /**
   * Handles tapping on a "Remove word" icon button.
   * @param {!{model: !{item: string}}} e
   */
  onRemoveWordTap_(e) {
    this.languageSettingsPrivate_.removeSpellcheckWord(e.model.item);
  },
});
