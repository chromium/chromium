// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Codepoints} from './types.js';

const LOCALSTORAGE_KEY = 'emoji-recently-used';
const MAX_RECENTS = 14;

/**
 * Recently used emoji, most recent first. Each emoji is stored as an array
 * of codepoints.
 * @typedef {!Array<Codepoints>}
 */
let RecentlyUsedEmoji;

/**
 * @return {RecentlyUsedEmoji} recently used emoji, most recent first.
 */
function load() {
  const stored = window.localStorage.getItem(LOCALSTORAGE_KEY);
  if (!stored) {
    return [];
  }
  return /** @type {?} */ (JSON.parse(stored));
}

/**
 * @param {RecentlyUsedEmoji} data recently used emoji, most recent first.
 */
function save(data) {
  window.localStorage.setItem(LOCALSTORAGE_KEY, JSON.stringify(data));
}

export class RecentEmojiStore {
  constructor() {
    this.data = load();
  }

  /**
   * Moves the given emoji to the front of the MRU list, inserting it if
   * it did not previously exist.
   * @param {!string} newEmoji most recently used emoji.
   */
  bumpEmoji(newEmoji) {
    // find and remove newEmoji from array if it previously existed.
    const oldIndex =
        this.data.findIndex(x => String.fromCodePoint(...x) === newEmoji);
    if (oldIndex !== -1) {
      this.data.splice(oldIndex, 1);
    }
    // insert newEmoji's codepoints to front of array.
    // first, split newEmoji into an array of strings where each string is
    // one codepoint. then, convert each codepoint to its numerical value.
    this.data.unshift([...newEmoji].map(x => x.codePointAt(0)));
    // slice from end of array if it exceeds MAX_RECENTS.
    if (this.data.length > MAX_RECENTS) {
      // setting length is sufficient to truncate an array.
      this.data.length = MAX_RECENTS;
    }
    save(this.data);
  }
}
