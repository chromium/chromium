// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {Token} from 'chrome://resources/mojo/mojo/public/mojom/base/token.mojom-webui.js';

import {RecentlyClosedTab, RecentlyClosedTabGroup, Tab, TabGroup} from './tab_search.mojom-webui.js';

/** @enum {number} */
export const TabItemType = {
  OPEN_TAB: 1,
  RECENTLY_CLOSED_TAB: 2,
  RECENTLY_CLOSED_TAB_GROUP: 3,
};

export class ItemData {
  constructor() {
    /** @type {boolean} */
    this.inActiveWindow;

    /** @type {!TabItemType} */
    this.type;

    /** @type {string} */
    this.a11yTypeText;

    /** @type {?TabGroup|?RecentlyClosedTabGroup} */
    this.tabGroup;

    /** @type {!Object} */
    this.highlightRanges = {};
  }
}

/**
 * TabData contains tabSearch.mojom.Tab and data derived from it.
 * It makes tabSearch.mojom.Tab immutable and works well for closure compiler
 * type checking.
 */
export class TabData extends ItemData {
  /**
   * @param {!Tab|!RecentlyClosedTab} tab
   * @param {!TabItemType} type
   */
  constructor(tab, type) {
    super();
    /** @type {!Tab|!RecentlyClosedTab} */
    this.tab = tab;

    this.type = type;

    /** @type {string} */
    this.hostname;
  }
}

export class TabGroupData extends ItemData {
  /** @param {!TabGroup|!RecentlyClosedTabGroup} tabGroup */
  constructor(tabGroup) {
    super();
    this.tabGroup = tabGroup;
    this.type = TabItemType.RECENTLY_CLOSED_TAB_GROUP;
  }
}

/**
 * Converts a token to a string by combining the high and low values as strings
 * with a hashtag as the separator.
 * @param {!Token} token
 * @return {string}
 */
export function tokenToString(token) {
  return `${token.high.toString()}#${token.low.toString()}`;
}

/**
 * @param {!Token} a
 * @param {!Token} b
 * @returns {boolean}
 */
export function tokenEquals(a, b) {
  return a.high === b.high && a.low === b.low;
}

/**
 * @param {!ItemData} itemData
 * @return {string}
 * @throws {Error}
 */
export function ariaLabel(itemData) {
  if (itemData instanceof TabGroupData &&
      itemData.type === TabItemType.RECENTLY_CLOSED_TAB_GROUP) {
    const tabGroup =
        /** @type {!RecentlyClosedTabGroup} */ (itemData.tabGroup);
    const tabCountText = loadTimeData.getStringF(
        tabGroup.tabCount == 1 ? 'oneTab' : 'tabCount', tabGroup.tabCount);
    return `${tabGroup.title} ${tabCountText} ${
        tabGroup.lastActiveElapsedText} ${itemData.a11yTypeText}`;
  }

  if (itemData instanceof TabData) {
    const tabData = /** @type {TabData} */ (itemData);
    const groupTitleOrEmpty = tabData.tabGroup ? tabData.tabGroup.title : '';
    return `${tabData.tab.title} ${groupTitleOrEmpty} ${tabData.hostname} ${
        tabData.tab.lastActiveElapsedText} ${tabData.a11yTypeText}`;
  }

  throw new Error('Invalid data provided.');
}
