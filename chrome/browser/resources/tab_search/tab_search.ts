// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './app.js';

export type {OptionKeyObject, Range, SearchOptions} from '/tab_search/shared/search.js';
export {search} from '/tab_search/shared/search.js';
export type {SearchApiProxy} from '/tab_search/shared/search_api_proxy.js';
export {SearchApiProxyImpl} from '/tab_search/shared/search_api_proxy.js';
export {getInstance as getAnnouncerInstance, TIMEOUT_MS} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
export {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
export {TabSearchAppElement} from './app.js';
export {SelectableLazyListElement} from './selectable_lazy_list.js';
export {SplitNewTabPageAppElement} from './split_view/app.js';
export {getHostname, getTabGroupTitle, getTitle, ItemData, SplitViewData, TabData, TabItemType, tokenToString} from './tab_data.js';
export {Color as TabGroupColor} from './tab_group_types.mojom-webui.js';
export type {ProfileData, RecentlyClosedTab, RecentlyClosedTabGroup, SwitchToTabInfo, Tab, TabGroup, TabsRemovedInfo, TabUpdateInfo, TokenRange, Window} from './tab_search.mojom-webui.js';
export {PageCallbackRouter, PageRemote, SplitTabLayout} from './tab_search.mojom-webui.js';
export type {TabSearchApiProxy} from './tab_search_api_proxy.js';
export {TabSearchApiProxyImpl} from './tab_search_api_proxy.js';
export {TabSearchGroupItemElement} from './tab_search_group_item.js';
export {TabSearchItemElement} from './tab_search_item.js';
export {SEARCH_QUERY_MAX_LENGTH, TabSearchPageElement, TabSearchUserAction} from './tab_search_page.js';
export {TabSearchSplitItemElement} from './tab_search_split_item.js';
export type {TabSearchSyncBrowserProxy} from './tab_search_sync_browser_proxy.js';
export {TabSearchSyncBrowserProxyImpl} from './tab_search_sync_browser_proxy.js';
export {TabAlertState} from './tabs.mojom-webui.js';
export {TitleItem} from './title_item.js';
