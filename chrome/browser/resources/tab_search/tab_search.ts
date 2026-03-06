// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './app.js';

export {getInstance as getAnnouncerInstance, TIMEOUT_MS} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
export {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
export {TabSearchAppElement} from './app.js';
export type {SearchOptions} from './search.js';
export {search} from './search.js';
export {SelectableLazyListElement} from './selectable_lazy_list.js';
export {SplitNewTabPageAppElement} from './split_view/app.js';
export {getHostname, getTabGroupTitle, getTitle, ItemData, TabData, TabItemType} from './tab_data.js';
export {Color as TabGroupColor} from './tab_group_types.mojom-webui.js';
export type {ProfileData, RecentlyClosedTab, RecentlyClosedTabGroup, SwitchToTabInfo, Tab, TabGroup, TabsRemovedInfo, TabUpdateInfo, Window} from './tab_search.mojom-webui.js';
export {PageCallbackRouter, PageRemote, TabSearchSection} from './tab_search.mojom-webui.js';
export type {TabSearchApiProxy} from './tab_search_api_proxy.js';
export {TabSearchApiProxyImpl} from './tab_search_api_proxy.js';
export {TabSearchGroupItemElement} from './tab_search_group_item.js';
export {TabSearchItemElement} from './tab_search_item.js';
export {SEARCH_QUERY_MAX_LENGTH, TabSearchPageElement} from './tab_search_page.js';
export type {TabSearchSyncBrowserProxy} from './tab_search_sync_browser_proxy.js';
export {TabSearchSyncBrowserProxyImpl} from './tab_search_sync_browser_proxy.js';
export {TabAlertState} from './tabs.mojom-webui.js';
export {TitleItem} from './title_item.js';
