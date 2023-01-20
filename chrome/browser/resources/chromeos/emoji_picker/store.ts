// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EmojiPickerApiProxy} from 'emoji_picker_api_proxy.js';

import {CategoryEnum, EmojiVariants, VisualContent} from './types.js';

const MAX_RECENTS = 10;

/**
 * @param {string} keyName Keyname of the object stored in storage
 * @return {{history:!Array<EmojiVariants>, preference:Object<string,string>}}
 *     recently used emoji, most recent first.
 */
function load(keyName: string) {
  const stored = window.localStorage.getItem(keyName);
  if (!stored) {
    return {history: [], preference: {}};
  }
  const parsed = /** @type {?} */ (JSON.parse(stored));
  // Throw out any old data
  return {history: parsed.history || [], preference: parsed.preference || {}};
}

/**
 * @param {{history:!Array<EmojiVariants>, preference:Object<string,string>}}
 *     data recently used emoji, most recent first.
 */
function save(
    keyName: string,
    data: {history: EmojiVariants[], preference: {[index: string]: string}}) {
  window.localStorage.setItem(keyName, JSON.stringify(data));
}

export class RecentlyUsedStore {
  storeName: string;
  data: {history: EmojiVariants[], preference: {[index: string]: string}};
  constructor(name: string) {
    this.storeName = name;
    this.data = load(name);
  }

  /**
   * Saves preferences for a base emoji.
   * returns True if any preferences are updated and false
   *    otherwise.
   */
  savePreferredVariant(baseEmoji: string, variant: string) {
    if (!baseEmoji) {
      return false;
    }

    // Base emoji must not be set as preference. So, store it only
    // if variant and baseEmoji are different and remove it from preference
    // otherwise.
    if (baseEmoji !== variant && variant) {
      this.data.preference[baseEmoji] = variant;
    } else if (baseEmoji in this.data.preference) {
      delete this.data.preference[baseEmoji];
    } else {
      return false;
    }

    save(this.storeName, this.data);
    return true;
  }

  getPreferenceMapping() {
    return this.data.preference;
  }

  clearRecents() {
    this.data.history = [];
    save(this.storeName, this.data);
  }

  /**
   * Moves the given item to the front of the MRU list, inserting it if
   * it did not previously exist.
   */
  bumpItem(category: CategoryEnum, newItem: EmojiVariants) {
    // Find and remove newItem from array if it previously existed.
    // Note, this explicitly allows for multiple recent item entries for the
    // same "base" emoji just with a different variant.
    let oldIndex;
    if (category === CategoryEnum.GIF) {
      oldIndex = this.data.history.findIndex(
          x =>
              (x.base.visualContent &&
               x.base.visualContent.id === newItem.base.visualContent?.id));
    } else {
      oldIndex = this.data.history.findIndex(
          x => (x.base.string && x.base.string === newItem.base.string));
    }

    if (oldIndex !== -1) {
      this.data.history.splice(oldIndex, 1);
    }
    // insert newItem to the front of the array.
    this.data.history.unshift(newItem);
    // slice from end of array if it exceeds MAX_RECENTS.
    if (this.data.history.length > MAX_RECENTS) {
      // setting length is sufficient to truncate an array.
      this.data.history.length = MAX_RECENTS;
    }
    save(this.storeName, this.data);
  }

  /**
   * Removes invalid GIFs from history.
   */
  async validate(apiProxy: EmojiPickerApiProxy): Promise<boolean> {
    if (this.data.history.length === 0) {
      // No GIFs to validate.
      return false;
    }

    // This function is only called on history items with visual content (i.e.
    // GIFs) so we can be confident an id will always exist.
    const ids = this.data.history.map(x => x.base.visualContent!.id);

    const {selectedGifs} = await apiProxy.getGifsByIds(ids);
    const map = new Map<string, VisualContent>();
    selectedGifs.forEach(gif => {
      map.set(gif.id, gif);
    });

    const validGifHistory =
        this.data.history.filter(item => map.has(item.base.visualContent!.id));
    const updated = (validGifHistory.length !== this.data.history.length);

    if (updated) {
      this.data.history = validGifHistory;
      save(this.storeName, this.data);
    }

    return updated;
  }
}
