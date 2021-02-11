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
 * @return {RecentlyUsedEmoji} recently used emoji, most recent first.
 */
function load() {
  const stored = window.localStorage.getItem(LOCALSTORAGE_KEY);
  if (!stored) {
    return [];
  }
  const parsed = /** @type {?} */ (JSON.parse(stored));
  if (parsed[0] && Array.isArray(parsed[0])) {
    // if stored data is in older codepoint format, ignore it.
    return [];
  }

  return parsed;
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
    const oldIndex = this.data.findIndex(x => x === newEmoji);
    if (oldIndex !== -1) {
      this.data.splice(oldIndex, 1);
    }
    // insert newEmoji to the front of the array.
    this.data.unshift(newEmoji);
    // slice from end of array if it exceeds MAX_RECENTS.
    if (this.data.length > MAX_RECENTS) {
      // setting length is sufficient to truncate an array.
      this.data.length = MAX_RECENTS;
    }
    save(this.data);
  }
}
