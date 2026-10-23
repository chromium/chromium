// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import type {BrowserProxy, ForeignTab} from '../foreign_tabs.mojom-webui.js';
import {browserProxyFactory} from '../foreign_tabs.mojom-webui.js';
import type {OrganizerListSectionClient, OrganizerListSectionDelegate} from '../organizer_list_section_delegate.js';
import type {OrganizerListSectionItem} from '../organizer_list_section_item.js';

import {getHostnameOrUrl} from './tab_delegate_utils.js';

export class ForeignTabsDelegate implements
    OrganizerListSectionDelegate<ForeignTab> {
  private browserProxy_: BrowserProxy = browserProxyFactory.getInstance();

  init(_sectionClient: OrganizerListSectionClient) {}

  getId(): string {
    return 'cross-device-tabs';
  }

  getHeader(): string {
    return loadTimeData.getString('tabsOnOtherDevices');
  }

  async getItems(): Promise<Array<OrganizerListSectionItem<ForeignTab>>> {
    const {tabs} = await this.browserProxy_.handler.getForeignTabs();

    return tabs.map(
        tab => ({
          title: [tab.title],
          description: [
            {text: tab.lastActiveElapsedText, matchable: false},
            {text: tab.deviceName},
          ],
          extraSearchTerms: tab.url ? [getHostnameOrUrl(tab.url)] : undefined,
          prefixIcon: tab.url ? {url: tab.url} : undefined,
          data: tab,
        }));
  }

  onItemClick(item: OrganizerListSectionItem<ForeignTab>) {
    const data = item.data;
    assert(data);
    this.browserProxy_.handler.openTab(data.sessionTag, data.sessionTabId);
  }
}
