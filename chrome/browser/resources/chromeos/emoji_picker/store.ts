// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EMOJI_PER_ROW} from './constants.js';
import {Category} from './emoji_picker.mojom-webui.js';
import {EmojiPickerApiProxy} from './emoji_picker_api_proxy.js';
import {CategoryEnum, Emoji, EmojiHistoryItem, EmojiVariants, Gender, PreferenceMapping, Tone, VisualContent} from './types.js';

const MAX_RECENTS = EMOJI_PER_ROW * 2;

// Covert CategoryEnum to Category type.
function convertCategoryEnum(category: CategoryEnum) {
  switch (category) {
    case CategoryEnum.EMOJI:
      return Category.kEmojis;
    case CategoryEnum.EMOTICON:
      return Category.kEmoticons;
    case CategoryEnum.SYMBOL:
      return Category.kSymbols;
    case CategoryEnum.GIF:
      return Category.kGifs;
  }
}

class Store<T> {
  data: T;

  /**
   * @param storageKey The key to use in local storage.
   * @param defaultData The initial data for this store. A new copy should
   *     be passed for each `Store` instance.
   */
  constructor(private readonly storageKey: string, defaultData: T) {
    this.data = this.load(defaultData);
  }

  /**
   * @param defaultData The initial data for this store. A new copy should
   *     be passed each time this is called.
   * @return The data from local storage if it exists, otherwise a reference
   *     to `defaultData`.
   */
  private load(defaultData: T): T {
    const stored = window.localStorage.getItem(this.storageKey);

    if (!stored) {
      return defaultData;
    }

    const parsed = JSON.parse(stored);

    // Checking for null because values of type 'object' can still be null.
    if (typeof defaultData !== 'object' || defaultData === null ||
        typeof parsed !== 'object' || parsed === null) {
      return parsed;
    }

    // Throw out any old data.
    const filteredEntries =
        Object.entries(parsed).filter(([key, _]) => key in defaultData);

    return {...defaultData, ...Object.fromEntries(filteredEntries)};
  }

  /**
   * Saves the existing data to local storage.
   */
  save() {
    window.localStorage.setItem(this.storageKey, JSON.stringify(this.data));
  }
}

interface RecentlyUsed {
  history: EmojiHistoryItem[];
  preference: PreferenceMapping;
}

export class RecentlyUsedStore {
  private store: Store<RecentlyUsed>;

  constructor(private readonly category: CategoryEnum) {
    this.store =
        new Store(`${category}-recently-used`, {history: [], preference: {}});
  }

  async mergeWithPrefsHistory() {
    if (this.category === CategoryEnum.GIF) {
      return;
    }
    const prefsHistory =
        await EmojiPickerApiProxy.getInstance().getHistoryFromPrefs(
            convertCategoryEnum(this.category));
    const mergedHistory: EmojiHistoryItem[] =
        prefsHistory.history.map((item) => ({
                                   base: {string: item.emoji},
                                   timestamp: item.timestamp.msec,
                                   alternates: [],
                                 }));
    for (const item of this.store.data.history) {
      const index = mergedHistory.findIndex(
          (emoji) => emoji.base.string === item.base.string);
      if (index >= 0) {
        item.timestamp = mergedHistory[index].timestamp;
        mergedHistory[index] = item;
      } else if (mergedHistory.length < MAX_RECENTS) {
        mergedHistory.push(item);
      }
    }
    this.store.data.history = mergedHistory;
    this.store.save();
    this.updateHistoryInPrefs();
  }

  /**
   * Saves preferences for a base emoji.
   * returns True if any preferences are updated and false
   *    otherwise.
   */
  savePreferredVariant(variant: string, baseEmoji?: string) {
    // If `baseEmoji === undefined`, then variant itself is a base emoji.
    if (!baseEmoji) {
      baseEmoji = variant;
    }

    const preference = this.store.data.preference;

    // Base emoji must not be set as preference. So, store it only
    // if variant and baseEmoji are different and remove it from preference
    // otherwise.
    if (baseEmoji !== variant && variant) {
      preference[baseEmoji] = variant;
    } else if (baseEmoji in preference) {
      delete preference[baseEmoji];
    } else {
      return false;
    }

    this.store.save();
    this.updatePreferredVariantsInPrefs();
    return true;
  }

  getHistory(): EmojiVariants[] {
    return this.store.data.history;
  }

  isHistoryEmpty(): boolean {
    return this.store.data.history.length === 0;
  }

  getPreferenceMapping(): PreferenceMapping {
    return this.store.data.preference;
  }

  clearRecents() {
    this.store.data.history = [];
    this.store.save();
    this.updateHistoryInPrefs();
  }

  clearItem(category: CategoryEnum, item: EmojiVariants) {
    const history = this.store.data.history;

    if (category === CategoryEnum.GIF) {
      this.store.data.history = history.filter(
          x =>
              (x.base.visualContent &&
               x.base.visualContent.id !== item.base.visualContent?.id));
    } else {
      this.store.data.history = history.filter(
          x => (x.base.string && x.base.string !== item.base.string));
    }
    this.store.save();
    this.updateHistoryInPrefs();
  }

  /**
   * Moves the given item to the front of the MRU list, inserting it if
   * it did not previously exist.
   */
  bumpItem(category: CategoryEnum, newItem: EmojiVariants) {
    const history = this.store.data.history;

    // Find and remove newItem from array if it previously existed.
    // Note, this explicitly allows for multiple recent item entries for the
    // same "base" emoji just with a different variant.
    let oldIndex;
    if (category === CategoryEnum.GIF) {
      oldIndex = history.findIndex(
          x =>
              (x.base.visualContent &&
               x.base.visualContent.id === newItem.base.visualContent?.id));
    } else {
      oldIndex = history.findIndex(
          x => (x.base.string && x.base.string === newItem.base.string));
    }

    if (oldIndex !== -1) {
      history.splice(oldIndex, 1);
    }

    const newHistoryItem: EmojiHistoryItem = newItem;
    newHistoryItem.timestamp = Date.now();
    // insert newItem to the front of the array.
    history.unshift(newHistoryItem);
    // slice from end of array if it exceeds MAX_RECENTS.
    if (history.length > MAX_RECENTS) {
      // setting length is sufficient to truncate an array.
      history.length = MAX_RECENTS;
    }
    this.store.save();
    this.updateHistoryInPrefs();
  }

  /**
   * Fills any gaps in the variant and grouping information for emojis with the
   * given name, because existing store data may not have the information.
   */
  fillEmojiVariantAttributes(
      name: string, alternates: Emoji[], groupedTone = false,
      groupedGender = false) {
    const matchingEmojis =
        this.store.data.history.filter(emoji => emoji.base.name === ' ' + name);

    if (matchingEmojis.length === 0) {
      return;
    }

    matchingEmojis.forEach(emoji => {
      emoji.alternates = alternates;
      emoji.groupedTone = groupedTone;
      emoji.groupedGender = groupedGender;
    });

    this.store.save();
  }

  updateHistoryInPrefs() {
    if (this.category !== CategoryEnum.GIF) {
      EmojiPickerApiProxy.getInstance().updateHistoryInPrefs(
          convertCategoryEnum(this.category),
          this.store.data.history.map((x) => ({
                                        emoji: x.base.string!,
                                        timestamp: {
                                          msec: x.timestamp || 0,
                                        },
                                      })));
    }
  }

  updatePreferredVariantsInPrefs() {
    if (this.category === CategoryEnum.EMOJI) {
      EmojiPickerApiProxy.getInstance().updatePreferredVariantsInPrefs(
          this.store.data.preference);
    }
  }

  /**
   * Removes invalid GIFs from history.
   */
  async validate(apiProxy: EmojiPickerApiProxy): Promise<boolean> {
    const history = this.store.data.history;

    if (history.length === 0) {
      // No GIFs to validate.
      return false;
    }

    // This function is only called on history items with visual content (i.e.
    // GIFs) so we can be confident an id will always exist.
    const ids = history.map(x => x.base.visualContent!.id);

    const {selectedGifs} = await apiProxy.getGifsByIds(ids);
    const map = new Map<string, VisualContent>();
    selectedGifs.forEach(gif => {
      map.set(gif.id, gif);
    });

    const validGifHistory =
        history.filter(item => map.has(item.base.visualContent!.id));
    const updated = (validGifHistory.length !== history.length);

    if (updated) {
      this.store.data.history = validGifHistory;
      this.store.save();
    }

    return updated;
  }
}

interface EmojiPreferences {
  tone: Tone|null;
  gender: Gender|null;
}

export class EmojiPreferencesStore {
  private store = new Store<EmojiPreferences>(
      'emoji-preferences', {tone: null, gender: null});

  getTone(): Tone|null {
    return this.store.data.tone;
  }

  setTone(tone: Tone) {
    this.store.data.tone = tone;
    this.store.save();
  }

  getGender(): Gender|null {
    return this.store.data.gender;
  }

  setGender(gender: Gender) {
    this.store.data.gender = gender;
    this.store.save();
  }
}

export class GifNudgeHistoryStore {
  private static store = new Store('emoji-picker-gif-nudge-shown', false);

  static hasNudgeShown(): boolean {
    return GifNudgeHistoryStore.store.data;
  }

  static setNudgeShown(value: boolean): void {
    GifNudgeHistoryStore.store.data = value;
    GifNudgeHistoryStore.store.save();
  }
}
