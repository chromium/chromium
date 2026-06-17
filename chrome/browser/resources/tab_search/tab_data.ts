// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {Token} from 'chrome://resources/mojo/mojo/public/mojom/base/token.mojom-webui.js';

import type {RecentlyClosedSplitView, RecentlyClosedTab, RecentlyClosedTabGroup, Tab, TabGroup} from './tab_search.mojom-webui.js';
import {SplitTabLayout} from './tab_search.mojom-webui.js';
import {tabHasMediaAlerts} from './tab_search_utils.js';
import type {Range} from './tab_search_utils.js';
import {TabAlertState} from './tabs.mojom-webui.js';

export enum TabItemType {
  OPEN_TAB = 1,
  RECENTLY_CLOSED_TAB = 2,
  RECENTLY_CLOSED_TAB_GROUP = 3,
  OPEN_SPLIT = 4,
  RECENTLY_CLOSED_SPLIT = 5,
}

export class ItemData {
  inActiveWindow: boolean = false;
  type: TabItemType = TabItemType.OPEN_TAB;
  a11yTypeText: string = '';
  tabGroup?: TabGroup|RecentlyClosedTabGroup;
  highlightRanges: Record<string, Range[]> = {};
}

/**
 * TabData contains tabSearch.mojom.Tab and data derived from it.
 * It makes tabSearch.mojom.Tab immutable and works well for TypeScript type
 * checking.
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
  override tabGroup: RecentlyClosedTabGroup;

  constructor(tabGroup: RecentlyClosedTabGroup) {
    super();
    this.tabGroup = tabGroup;
    this.type = TabItemType.RECENTLY_CLOSED_TAB_GROUP;
  }
}

export class SplitViewData extends ItemData {
  splitView?: RecentlyClosedSplitView;
  tabs?: [Tab, Tab];

  constructor(options: {
    splitView?: RecentlyClosedSplitView,
    tabs?: [Tab, Tab],
  }) {
    super();
    if (!options.splitView && !options.tabs) {
      throw new Error(
          'SplitViewData must be initialized with splitView or tabs.');
    }
    this.splitView = options.splitView;
    this.tabs = options.tabs;
    this.type = options.splitView ? TabItemType.RECENTLY_CLOSED_SPLIT :
                                    TabItemType.OPEN_SPLIT;
    this.a11yTypeText =
        options.splitView ? 'Recently closed split view' : 'Open split view';
  }

  get title(): string {
    return 'Split View';
  }

  get sessionId(): number {
    return this.splitView ? this.splitView.sessionId : -1;
  }

  get splitId(): Token|undefined {
    if (this.splitView) {
      return this.splitView.id;
    }
    return this.tabs![0].splitId ?? undefined;
  }

  get tabCount(): number {
    return this.splitView ? this.splitView.tabCount : 2;
  }

  get lastActiveElapsedText(): string {
    if (this.splitView) {
      return this.splitView.lastActiveElapsedText;
    }
    return this.tabs![0].lastActiveElapsedText;
  }

  get tabUrls(): string[] {
    if (this.splitView) {
      return this.splitView.tabUrls;
    }
    return [this.tabs![0].url, this.tabs![1].url];
  }

  get layout(): SplitTabLayout {
    if (this.splitView) {
      return this.splitView.layout;
    }
    return this.tabs![0].splitLayout ?? SplitTabLayout.kSideBySide;
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
      case TabAlertState.kAudioRecording:
        return loadTimeData.getStringF('audioRecording', tabTitle);
      case TabAlertState.kVideoRecording:
        return loadTimeData.getStringF('videoRecording', tabTitle);
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
    const tabGroup = itemData.tabGroup;
    const tabCountText = loadTimeData.getStringF(
        tabGroup.tabCount === 1 ? 'oneTab' : 'tabCount', tabGroup.tabCount);
    return `${tabGroup.title} ${tabCountText} ${
        tabGroup.lastActiveElapsedText} ${itemData.a11yTypeText}`;
  }

  if (itemData instanceof SplitViewData) {
    const hostnames = itemData.tabUrls.slice(0, 2).map(
        url => getDisplayHostnameForUrl(new URL(normalizeURL(url))));
    const lastActive = itemData.lastActiveElapsedText;
    const lastActivePart = lastActive ? `, ${lastActive}` : '';
    return `Split view, ${hostnames[0]}, ${hostnames[1]}${lastActivePart}. ${
        itemData.a11yTypeText}`;
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

export function normalizeURL(url: string): string {
  // When a navigation is cancelled before completion, the tab's URL can be
  // empty, which leads to errors when attempting to construct a URL object with
  // it. To handle this, we substitute any empty URL with 'about:blank'. This is
  // consistent with how the Omnibox handles empty URLs.
  return url || 'about:blank';
}

export function getDisplayHostnameForUrl(url: URL): string {
  if (url.protocol === 'blob:') {
    return loadTimeData.getString('blobUrlSource');
  } else if (url.protocol === 'file:') {
    return loadTimeData.getString('fileUrlSource');
  } else if (url.protocol === 'about:' && url.pathname === 'blank') {
    return 'about:blank';
  } else if (url.protocol === 'view-source:') {
    return loadTimeData.getString('viewSourceSource');
  } else {
    return url.hostname;
  }
}

export function getTitle(data: TabData|TabGroupData|SplitViewData): string|
    undefined {
  if (data.type === TabItemType.RECENTLY_CLOSED_TAB_GROUP) {
    return undefined;
  }

  if (data instanceof SplitViewData) {
    if (data.tabs) {
      // Returns "Split View" followed by the joined titles of both sub-tabs to
      // allow search matching against either "Split View" or either tab's
      // title.
      return `${data.title} ${data.tabs[0].title} ${data.tabs[1].title}`;
    }
    return data.title;
  }

  return (data as TabData).tab.title;
}

export function getHostname(data: TabData|TabGroupData|SplitViewData): string|
    undefined {
  if (data.type === TabItemType.RECENTLY_CLOSED_TAB_GROUP) {
    return undefined;
  }

  if (data instanceof SplitViewData) {
    // Returns the joined display hostnames of both sub-tabs to allow search
    // matching against either tab's URL/domain.
    return data.tabUrls
        .map(url => {
          try {
            return getDisplayHostnameForUrl(new URL(normalizeURL(url)));
          } catch (e) {
            return 'about:blank';
          }
        })
        .join(' ');
  }

  return (data as TabData).hostname;
}

export function getTabGroupTitle(data: TabData|TabGroupData|SplitViewData):
    string|undefined {
  return data.tabGroup?.title;
}
