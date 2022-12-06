// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {Token} from 'chrome://resources/mojo/mojo/public/mojom/base/token.mojom-webui.js';

import {RecentlyClosedTab, RecentlyClosedTabGroup, Tab, TabGroup} from './tab_search.mojom-webui.js';
import {tabHasMediaAlerts} from './tab_search_utils.js';
import {TabAlertState} from './tabs.mojom-webui.js';

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
  hostname: string;

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

function titleAndAlertAriaLabel(tabData: TabData): string {
  const tabTitle = tabData.tab.title;
  if (tabData.type === TabItemType.OPEN_TAB &&
      tabHasMediaAlerts(tabData.tab as Tab)) {
    // GetTabAlertStatesForContents adds alert indicators in the order of their
    // priority. Only relevant media alerts are sent over mojo so the first
    // element in alertStates will be the highest priority media alert to
    // display.
    const alert = (tabData.tab as Tab).alertStates[0];
    switch (alert) {
      case TabAlertState.kMediaRecording:
        return loadTimeData.getStringF('mediaRecording', tabTitle);
      case TabAlertState.kAudioPlaying:
        return loadTimeData.getStringF('audioPlaying', tabTitle);
      case TabAlertState.kAudioMuting:
        return loadTimeData.getStringF('audioMuting', tabTitle);
      default:
        return tabTitle;
    }
  }
  return tabTitle;
}

export function ariaLabel(itemData: ItemData): string {
  if (itemData instanceof TabGroupData &&
      itemData.type === TabItemType.RECENTLY_CLOSED_TAB_GROUP) {
    const tabGroup = itemData.tabGroup as RecentlyClosedTabGroup;
    const tabCountText = loadTimeData.getStringF(
        tabGroup.tabCount === 1 ? 'oneTab' : 'tabCount', tabGroup.tabCount);
    return `${tabGroup.title} ${tabCountText} ${
        tabGroup.lastActiveElapsedText} ${itemData.a11yTypeText}`;
  }

  if (itemData instanceof TabData) {
    const tabData = itemData;
    const groupTitleOrEmpty = tabData.tabGroup ? tabData.tabGroup.title : '';
    const titleAndAlerts = titleAndAlertAriaLabel(tabData);
    return `${titleAndAlerts} ${groupTitleOrEmpty} ${tabData.hostname} ${
        tabData.tab.lastActiveElapsedText} ${tabData.a11yTypeText}`;
  }

  throw new Error('Invalid data provided.');
}
