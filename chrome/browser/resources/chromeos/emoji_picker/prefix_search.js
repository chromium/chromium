// Copyright 2022 The Chromium Authors
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
    for (const record of collection) {
      const string = record.base.string;
      const name = record.base.name;
      const terms = this.tokenize_(name).map(term => this.sanitize_(term));
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
    const results = new Set();
    const terms = this.tokenTrie_.getKeys(prefix);
    for (const term of terms) {
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

  /**
   * Split a phrase into tokens.
   * @private
   * @param {string} phrase
   * @returns {!Array<string>} array of non-empty tokens.
   */
  tokenize_(phrase) {
    return phrase.split(' ').filter(token => token.length > 0);
  }

  /**
   * Fetch all words from the emoji data that have 'term' has prefix and attach
   * matching metadata for each word.
   * @param {!EmojiVariants} emoji
   * @param {string} term
   * @returns {!Array<{pos: Number, isMatched: Boolean, token: String, weight:
   *     Number}>} Array of matching metadata.
   */
  getMatchedKeywords_(emoji, term) {
    const PRIMARY_NAME_WEIGHT = 1;
    return this.tokenize_(this.sanitize_(emoji.base.name))
        .map((token, pos) => ({
               pos,
               isMatched: token.startsWith(term),
               token,
               weight: PRIMARY_NAME_WEIGHT,
             }))
        .filter(item => item.isMatched);
  }

  /**
   * Calculate the matching score of a term against a given emoji.
   * @param {!EmojiVariants} emoji
   * @param {string} term
   * @throws Thrown when any matched word from emoji description is empty.
   * @returns {number}
   */
  scoreTermAgainstEmoji(emoji, term) {
    let score = 0;
    for (const item of this.getMatchedKeywords_(emoji, term)) {
      if (item.token.length === 0) {
        throw new Error('Token can not be empty.');
      }
      // Link to single-word match score formula:
      // https://docs.google.com/document/d/1Ub89xsElqVyRaq8tldhligd29-iXsSMWlAdL6Q-Xpr8/edit#
      score +=
          (item.weight / (1 + item.pos)) * (term.length / item.token.length);
    }
    return score;
  }

  /**
   * Search for all items that match with the given query
   * @param {string} query multi-word query
   * @returns {!Array<{item: !EmojiVariants, score: number}>} an array of
   *     matched items.
   */
  search(query) {
    const queryScores = new Map();
    const sanitizedQuery = this.sanitize_(query);
    this.tokenize_(sanitizedQuery).forEach((term, idx) => {
      // For each token
      const termScores = new Map();
      const candidateEmojis = this.matchPrefixToEmojis(term);

      for (const emoji of candidateEmojis) {
        const emojiRecord = this.emojiMap_.get(emoji);
        termScores.set(emoji, this.scoreTermAgainstEmoji(emojiRecord, term));
      }

      for (const emoji of termScores.keys()) {
        // If it is the first term in the query phrase, we apply the
        // normalization factor.
        if (idx === 0) {
          const emojiName = this.emojiMap_.get(emoji).base.name;
          queryScores.set(emoji, sanitizedQuery.length / emojiName.length);
        }
        if (queryScores.has(emoji)) {
          queryScores.set(
              emoji, queryScores.get(emoji) * termScores.get(emoji));
        }
      }

      // Remove any emoji at query level if it does not match at term level.
      for (const emoji of queryScores.keys()) {
        if (!termScores.has(emoji)) {
          queryScores.delete(emoji);
        }
      }
    });

    const results =
        Array.from(queryScores.keys()).map(emoji => ({
                                             item: this.emojiMap_.get(emoji),
                                             score: queryScores.get(emoji),
                                           }));
    return this.sort_(results);
  }

  /**
   * Sort the array of Emoji objects by relevance score in descending order
   * @param {!Array<{item: !EmojiVariants, score: number}>} results
   * @returns {!Array<{item: !EmojiVariants, score: number}>} the sorted array
   *     of Emoji objects.
   */
  sort_(results) {
    return results.sort((emoji1, emoji2) => emoji2.score - emoji1.score);
  }
}