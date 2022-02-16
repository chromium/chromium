// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Trie} from './structs/trie.js';
import {EmojiVariants} from './types.js';

export class EmojiPrefixSearch {
  constructor() {
    /** @type {!Trie} */
    this.tokenTrie_ = new Trie();

    /** @type {!Map<string, !Set<string>>} */
    this.wordToEmojisMap_ = new Map();

    /** @type {!Map<string, EmojiVariants>} */
    this.emojiMap_ = new Map();
  }

  /**
   * @param {!Array<!EmojiVariants>} collection
   */
  setCollection(collection) {
    this.clear();
    for (let record of collection) {
      const string = record.base.string;
      const name = record.base.name;
      const terms = name.split(' ').map(term => this.sanitize_(term));
      terms.forEach(term => {
        if (!this.wordToEmojisMap_.has(term)) {
          this.wordToEmojisMap_.set(term, new Set());
        }
        this.wordToEmojisMap_.get(term).add(string);
        this.emojiMap_.set(string, record);
        this.tokenTrie_.add(term);
      });
    }
  }

  /**
   * Returns all items whose name contains the prefix.
   * @param {string} prefix
   * @returns {!Array<string>}
   */
  matchPrefixToEmojis(prefix) {
    let results = new Set();
    const terms = this.tokenTrie_.getKeys(prefix);
    for (let term of terms) {
      const matchedItems = this.wordToEmojisMap_.get(term);
      if (matchedItems !== undefined) {
        matchedItems.forEach(item => {
          results.add(item);
        });
      }
    }
    return Array.from(results);
  }

  /**
   * Preprocess a phrase by the following operations:
   *  (1) remove white whitespace at both ends of the phrase.
   *  (2) convert all letters into lowercase.
   * @param {string} phrase
   * @returns {string}
   */
  sanitize_(phrase) {
    return phrase.trim().toLowerCase();
  }

  /**
   * Clear trie and lookup table.
   */
  clear() {
    this.tokenTrie_.clear();
    this.wordToEmojisMap_.clear();
    this.emojiMap_.clear();
  }
}