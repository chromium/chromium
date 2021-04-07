// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const LOCALSTORAGE_KEY = 'emoji-recently-used';
const MAX_RECENTS = 18;

/**
 * Recently used emoji, most recent first. Each emoji is stored as a string.
 * @typedef {!Array<string>} RecentlyUsedEmoji
 */
let RecentlyUsedEmoji;

/**
 * @return {{history:RecentlyUsedEmoji, preference:Object<string,string>}}
 *     recently used emoji, most recent first.
 */
function load() {
  const stored = window.localStorage.getItem(LOCALSTORAGE_KEY);
  if (!stored) {
    return {history: [], preference: {}};
  }
  const parsed = /** @type {?} */ (JSON.parse(stored));
  // Throw out any old data
  return {history: parsed.history || [], preference: parsed.preference || {}};
}

/**
 * @param {{history:RecentlyUsedEmoji, preference:Object<string,string>}} data
 *     recently used emoji, most recent first.
 */
function save(data) {
  window.localStorage.setItem(LOCALSTORAGE_KEY, JSON.stringify(data));
}

export class RecentEmojiStore {
  constructor() {
    this.data = load();
  }

  savePreferredVariant(baseEmoji, variant) {
    if (!baseEmoji) {
      return;
    }
    this.data.preference[baseEmoji] = variant;
    save(this.data);
  }

  getPreferenceMapping() {
    return this.data.preference;
  }

  clearRecents() {
    this.data.history = [];
    save(this.data);
  }

  /**
   * Moves the given emoji to the front of the MRU list, inserting it if
   * it did not previously exist.
   * @param {!string} newEmoji most recently used emoji.
   */
  bumpEmoji(newEmoji) {
    // find and remove newEmoji from array if it previously existed.
    const oldIndex = this.data.history.findIndex(x => x === newEmoji);
    if (oldIndex !== -1) {
      this.data.history.splice(oldIndex, 1);
    }
    // insert newEmoji to the front of the array.
    this.data.history.unshift(newEmoji);
    // slice from end of array if it exceeds MAX_RECENTS.
    if (this.data.history.length > MAX_RECENTS) {
      // setting length is sufficient to truncate an array.
      this.data.history.length = MAX_RECENTS;
    }
    save(this.data);
  }
}
