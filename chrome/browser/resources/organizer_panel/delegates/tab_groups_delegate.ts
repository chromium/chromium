// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/tab_group_shared/tab_group_dot.js';

import {CrUrlListItemSize} from '//resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {html} from '//resources/lit/v3_0/lit.rollup.js';
import {TabGroupDotSize} from '/tab_group_shared/tab_group_dot.js';

import type {OrganizerListSectionClient, OrganizerListSectionDelegate} from '../organizer_list_section_delegate.js';
import type {OrganizerListSectionItem} from '../organizer_list_section_item.js';
import type {BrowserProxy, TabGroup} from '../tab_groups.mojom-webui.js';
import {browserProxyFactory} from '../tab_groups.mojom-webui.js';

export class TabGroupsDelegate implements
    OrganizerListSectionDelegate<TabGroup> {
  private browserProxy_: BrowserProxy = browserProxyFactory.getInstance();

  init(_sectionClient: OrganizerListSectionClient) {}

  getHeader(): string {
    return loadTimeData.getString('tabGroups');
  }

  async getItems(): Promise<Array<OrganizerListSectionItem<TabGroup>>> {
    const {tabGroups} = await this.browserProxy_.handler.getTabGroups();
    return tabGroups.map(group => this.tabGroupToSectionItem_(group));
  }

  onItemClick(_item: OrganizerListSectionItem<TabGroup>) {}

  private tabGroupToSectionItem_(group: TabGroup):
      OrganizerListSectionItem<TabGroup> {
    return {
      title: [group.title],
      prefixIcon: {
        element: html`<tab-group-dot .color="${group.color}"
            .size="${TabGroupDotSize.LARGE}"></tab-group-dot>`,
      },
      size: CrUrlListItemSize.COMPACT,
      data: group,
    };
  }
}
