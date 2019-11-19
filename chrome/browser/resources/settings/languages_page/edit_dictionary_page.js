// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-edit-dictionary-page' is a sub-page for editing
 * the "dictionary" of custom words used for spell check.
 */

// Max valid word size defined in
// https://cs.chromium.org/chromium/src/components/spellcheck/common/spellcheck_common.h?l=28
const MAX_CUSTOM_DICTIONARY_WORD_BYTES = 99;

Polymer({
  is: 'settings-edit-dictionary-page',

  behaviors: [settings.GlobalScrollTargetBehavior],

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
      value: settings.routes.EDIT_DICTIONARY,
    },

    /** @private {!Array<string>} */
    words_: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /** @private {boolean} */
    hasWords_: {
      type: Boolean,
      value: false,
    },
  },

  /** @type {LanguageSettingsPrivate} */
  languageSettingsPrivate: null,

  /** @override */
  ready: function() {
    this.languageSettingsPrivate = settings.languageSettingsPrivateApiForTest ||
        /** @type {!LanguageSettingsPrivate} */
        (chrome.languageSettingsPrivate);

    this.languageSettingsPrivate.getSpellcheckWords(words => {
      this.hasWords_ = words.length > 0;
      this.words_ = words;
    });

    this.languageSettingsPrivate.onCustomDictionaryChanged.addListener(
        this.onCustomDictionaryChanged_.bind(this));

    // Add a key handler for the new-word input.
    this.$.keys.target = this.$.newWord;
  },

  /**
   * Adds the word in the new-word input to the dictionary.
   * @private
   */
  addWordFromInput_: function() {
    // Spaces are allowed, but removing leading and trailing whitespace.
    const word = this.getTrimmedNewWord_();
    this.newWordValue_ = '';
    if (word) {
      this.languageSettingsPrivate.addSpellcheckWord(word);
    }
  },

  /**
   * Check if the field is empty or invalid.
   * @return {boolean}
   * @private
   */
  disableAddButton_: function() {
    return this.getTrimmedNewWord_().length == 0 || this.isWordInvalid_();
  },

  /**
   * @return {string}
   * @private
   */
  getErrorMessage_: function() {
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
  getTrimmedNewWord_: function() {
    return this.newWordValue_.trim();
  },

  /**
   * If the word is invalid, returns true (or a message if one is provided).
   * Otherwise returns false.
   * @return {boolean}
   * @private
   */
  isWordInvalid_: function() {
    return this.newWordAlreadyAdded_() || this.newWordIsTooLong_();
  },

  /**
   * @return {boolean}
   * @private
   */
  newWordAlreadyAdded_: function() {
    return this.words_.includes(this.getTrimmedNewWord_());
  },

  /**
   * @return {boolean}
   * @private
   */
  newWordIsTooLong_: function() {
    return this.getTrimmedNewWord_().length > MAX_CUSTOM_DICTIONARY_WORD_BYTES;
  },

  /**
   * Handles tapping on the Add Word button.
   */
  onAddWordTap_: function(e) {
    this.addWordFromInput_();
    this.$.newWord.focus();
  },

  /**
   * Handles updates to the word list. Additions are unshifted to the top
   * of the list so that users can see them easily.
   * @param {!Array<string>} added
   * @param {!Array<string>} removed
   */
  onCustomDictionaryChanged_: function(added, removed) {
    const wasEmpty = this.words_.length == 0;

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
      Polymer.dom.flush();
      this.$$('#list').notifyResize();
    }
  },

  /**
   * Handles Enter and Escape key presses for the new-word input.
   * @param {!CustomEvent<!{key: string}>} e
   */
  onKeysPress_: function(e) {
    if (e.detail.key == 'enter' && !this.disableAddButton_()) {
      this.addWordFromInput_();
    } else if (e.detail.key == 'esc') {
      e.detail.keyboardEvent.target.value = '';
    }
  },

  /**
   * Handles tapping on a "Remove word" icon button.
   * @param {!{model: !{item: string}}} e
   */
  onRemoveWordTap_: function(e) {
    this.languageSettingsPrivate.removeSpellcheckWord(e.model.item);
  },
});
