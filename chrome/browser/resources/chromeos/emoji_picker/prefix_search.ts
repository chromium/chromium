// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Trie} from './structs/trie.js';
import {EmojiVariants} from './types.js';

/**
 * Preprocess a phrase by the following operations:
 *  (1) remove white whitespace at both ends of the phrase.
 *  (2) convert all letters into lowercase.
 */
function sanitize(phrase: string) {
  return phrase.trim().toLowerCase();
}

export class EmojiPrefixSearch {
  private tokenTrie = new Trie();
  private wordToEmojisMap: Map<string, Set<string>> = new Map();
  private emojiMap: Map<string, EmojiVariants> = new Map();

  setCollection(collection: EmojiVariants[]) {
    this.clear();
    for (const record of collection) {
      if (!record.base.string || !record.base.name) {
        continue;
      }
      const string = record.base.string;
      const name = record.base.name;
      const terms = this.tokenize(name).map(term => sanitize(term));
      terms.forEach(term => {
        if (!this.wordToEmojisMap.has(term)) {
          this.wordToEmojisMap.set(term, new Set());
        }
        this.wordToEmojisMap.get(term)?.add(string);
        this.emojiMap.set(string, record);
        this.tokenTrie.add(term);
      });
    }
  }

  /**
   * Returns all items whose name contains the prefix.
   */
  matchPrefixToEmojis(prefix: string): string[] {
    const results: Set<string> = new Set();
    const terms = this.tokenTrie.getKeys(prefix);
    for (const term of terms) {
      const matchedItems = this.wordToEmojisMap.get(term);
      if (matchedItems !== undefined) {
        matchedItems.forEach(item => {
          results.add(item);
        });
      }
    }
    return Array.from(results);
  }

  /**
   * Clear trie and lookup table.
   */
  clear(): void {
    this.tokenTrie.clear();
    this.wordToEmojisMap.clear();
    this.emojiMap.clear();
  }

  private tokenize(phrase: string): string[] {
    return phrase.split(' ').filter(token => token.length > 0);
  }

  /**
   * Fetch all words from the emoji data that have 'term' has prefix and attach
   * matching metadata for each word.
   */
  getMatchedKeywords(emoji: EmojiVariants, term: string): Array<{
    pos: number,
    isMatched: boolean,
    token: string,
    weight: number,
  }> {
    if (!emoji.base.name) {
      return [];
    }
    const PRIMARY_NAME_WEIGHT = 1;
    return this.tokenize(sanitize(emoji.base.name))
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
   */
  scoreTermAgainstEmoji(emoji: EmojiVariants, term: string) {
    let score = 0;
    for (const item of this.getMatchedKeywords(emoji, term)) {
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
   */
  search(query: string) {
    const queryScores = new Map();
    const sanitizedQuery = sanitize(query);
    this.tokenize(sanitizedQuery).forEach((term, idx) => {
      // For each token
      const termScores = new Map();
      const candidateEmojis = this.matchPrefixToEmojis(term);

      for (const emoji of candidateEmojis) {
        const emojiRecord = this.emojiMap.get(emoji);
        if (emojiRecord) {
          termScores.set(emoji, this.scoreTermAgainstEmoji(emojiRecord, term));
        }
      }

      for (const emoji of termScores.keys()) {
        // If it is the first term in the query phrase, we apply the
        // normalization factor.
        if (idx === 0) {
          const emojiName = this.emojiMap.get(emoji)?.base?.name ?? ' ';
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
                                             item: this.emojiMap.get(emoji) as
                                                 EmojiVariants,
                                             score: queryScores.get(emoji),
                                           }));
    return this.sort(results);
  }

  /**
   * Sort the array of Emoji objects by relevance score in descending order
   */
  private sort(results: Array<{item: EmojiVariants, score: number}>) {
    return results.sort((emoji1, emoji2) => emoji2.score - emoji1.score);
  }
}