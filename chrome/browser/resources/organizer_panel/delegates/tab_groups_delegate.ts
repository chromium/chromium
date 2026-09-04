// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrUrlListItemSize} from '//resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OrganizerListSectionClient, OrganizerListSectionDelegate} from '../organizer_list_section_delegate.js';
import type {OrganizerListSectionItem} from '../organizer_list_section_item.js';
import {Color} from '../tab_group_types.mojom-webui.js';
import type {TabGroup} from '../tab_search.mojom-webui.js';

export class TabGroupsDelegate implements
    OrganizerListSectionDelegate<TabGroup> {
  private tabGroups_: TabGroup[] = [
    {
      id: {high: 0n, low: 1n},
      color: Color.kBlue,
      title: 'Sample Group 1',
    },
    {
      id: {high: 0n, low: 2n},
      color: Color.kRed,
      title: 'Sample Group 2',
    },
    {
      id: {high: 0n, low: 3n},
      color: Color.kGreen,
      title: 'Sample Group 3',
    },
  ];

  init(_sectionClient: OrganizerListSectionClient) {}

  getHeader(): string {
    return loadTimeData.getString('tabGroups');
  }

  getItems(): Promise<Array<OrganizerListSectionItem<TabGroup>>> {
    return Promise.resolve(
        this.tabGroups_.map(group => this.tabGroupToSectionItem_(group)));
  }

  onItemClick(_item: OrganizerListSectionItem<TabGroup>) {}

  private tabGroupToSectionItem_(group: TabGroup):
      OrganizerListSectionItem<TabGroup> {
    // TODO(crbug.com/556738439): Share color logic with tab search.
    return {
      title: group.title,
      prefixIcon: {
        element: html`
          <svg viewBox="-5 -5 10 10"
              xmlns="http://www.w3.org/2000/svg"
              display="block" style="height: 8px; width: 8px">
            <circle cx="0" cy="0" r="4">
            </circle>
          </svg>
        `,
      },
      size: CrUrlListItemSize.COMPACT,
      data: group,
    };
  }
}
