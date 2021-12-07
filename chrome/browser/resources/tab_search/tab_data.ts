// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {Token} from 'chrome://resources/mojo/mojo/public/mojom/base/token.mojom-webui.js';

import {RecentlyClosedTab, RecentlyClosedTabGroup, Tab, TabGroup} from './tab_search.mojom-webui.js';

export enum TabItemType {
  OPEN_TAB = 1,
  RECENTLY_CLOSED_TAB = 2,
  RECENTLY_CLOSED_TAB_GROUP = 3,
}

export class ItemData {
  inActiveWindow: boolean;
  type: TabItemType;
  a11yTypeText: string;
  tabGroup?: TabGroup|RecentlyClosedTabGroup;
  highlightRanges: {[key: string]: Array<{start: number, length: number}>} = {};
}

/**
 * TabData contains tabSearch.mojom.Tab and data derived from it.
 * It makes tabSearch.mojom.Tab immutable and works well for closure compiler
 * type checking.
 */
export class TabData extends ItemData {
  tab: Tab|RecentlyClosedTab;
  hostname: string

  constructor(tab: Tab|RecentlyClosedTab, type: TabItemType, hostname: string) {
    super();
    this.tab = tab;
    this.type = type;
    this.hostname = hostname;
  }
}

export class TabGroupData extends ItemData {
  constructor(tabGroup: TabGroup|RecentlyClosedTabGroup) {
    super();
    this.tabGroup = tabGroup;
    this.type = TabItemType.RECENTLY_CLOSED_TAB_GROUP;
  }
}

/**
 * Converts a token to a string by combining the high and low values as strings
 * with a hashtag as the separator.
 */
export function tokenToString(token: Token): string {
  return `${token.high.toString()}#${token.low.toString()}`;
}

export function tokenEquals(a: Token, b: Token): boolean {
  return a.high === b.high && a.low === b.low;
}

export function ariaLabel(itemData: ItemData): string {
  if (itemData instanceof TabGroupData &&
      itemData.type === TabItemType.RECENTLY_CLOSED_TAB_GROUP) {
    const tabGroup = itemData.tabGroup as RecentlyClosedTabGroup;
    const tabCountText = loadTimeData.getStringF(
        tabGroup.tabCount == 1 ? 'oneTab' : 'tabCount', tabGroup.tabCount);
    return `${tabGroup.title} ${tabCountText} ${
        tabGroup.lastActiveElapsedText} ${itemData.a11yTypeText}`;
  }

  if (itemData instanceof TabData) {
    const tabData = itemData;
    const groupTitleOrEmpty = tabData.tabGroup ? tabData.tabGroup.title : '';
    return `${tabData.tab.title} ${groupTitleOrEmpty} ${tabData.hostname} ${
        tabData.tab.lastActiveElapsedText} ${tabData.a11yTypeText}`;
  }

  throw new Error('Invalid data provided.');
}
