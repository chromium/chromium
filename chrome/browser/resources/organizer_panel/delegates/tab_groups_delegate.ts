// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/tab_group_shared/tab_group_dot.js';

import {CrUrlListItemSize} from '//resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {Uuid} from '//resources/mojo/mojo/public/mojom/base/uuid.mojom-webui.js';
import {TabGroupDotSize} from '/tab_group_shared/tab_group_dot.js';

import type {OrganizerListSectionClient, OrganizerListSectionDelegate} from '../organizer_list_section_delegate.js';
import type {OrganizerListSectionItem} from '../organizer_list_section_item.js';
import type {BrowserProxy, TabGroup} from '../tab_groups.mojom-webui.js';
import {browserProxyFactory} from '../tab_groups.mojom-webui.js';

export class TabGroupsDelegate implements
    OrganizerListSectionDelegate<TabGroup> {
  private browserProxy_: BrowserProxy = browserProxyFactory.getInstance();
  private client_?: OrganizerListSectionClient;
  private listenerIds_: number[] = [];
  private tabGroups_: TabGroup[] = [];

  init(sectionClient: OrganizerListSectionClient) {
    this.client_ = sectionClient;
    const callbackRouter = this.browserProxy_.callbackRouter;
    this.listenerIds_.push(
        callbackRouter.tabGroupAdded.addListener((tabGroup: TabGroup) => {
          this.onTabGroupAdded_(tabGroup);
        }),
        callbackRouter.tabGroupRemoved.addListener((id: Uuid) => {
          this.onTabGroupRemoved_(id);
        }),
        callbackRouter.tabGroupUpdated.addListener((tabGroup: TabGroup) => {
          this.onTabGroupUpdated_(tabGroup);
        }));
  }

  getHeader(): string {
    return loadTimeData.getString('tabGroups');
  }

  async getItems(): Promise<Array<OrganizerListSectionItem<TabGroup>>> {
    const {tabGroups} = await this.browserProxy_.handler.getTabGroups();
    this.tabGroups_ = tabGroups;
    return this.tabGroups_.map(group => this.tabGroupToSectionItem_(group));
  }

  onItemClick(item: OrganizerListSectionItem<TabGroup>) {
    const data = item.data;
    assert(data);
    this.browserProxy_.handler.openTabGroup(data.id);
  }

  async onItemActionButtonClicked(
      item: OrganizerListSectionItem<TabGroup>, buttonElement: HTMLElement) {
    assert(item.data);
    const rect = buttonElement.getBoundingClientRect();
    await this.browserProxy_.handler.showContextMenu(item.data.id, {
      x: Math.round(rect.x),
      y: Math.round(rect.y),
      width: Math.round(rect.width),
      height: Math.round(rect.height),
    });
  }

  private notifyClient_() {
    this.client_?.onItemsChanged(
        this.tabGroups_.map(group => this.tabGroupToSectionItem_(group)));
  }

  private onTabGroupAdded_(tabGroup: TabGroup) {
    this.tabGroups_ = [
      tabGroup,
      ...this.tabGroups_.filter(g => g.id.value !== tabGroup.id.value),
    ];
    this.notifyClient_();
  }

  private onTabGroupRemoved_(id: Uuid) {
    this.tabGroups_ = this.tabGroups_.filter(g => g.id.value !== id.value);
    this.notifyClient_();
  }

  private onTabGroupUpdated_(tabGroup: TabGroup) {
    const index =
        this.tabGroups_.findIndex(g => g.id.value === tabGroup.id.value);
    if (index !== -1) {
      this.tabGroups_[index] = tabGroup;
      this.notifyClient_();
    }
  }

  private tabGroupToSectionItem_(group: TabGroup):
      OrganizerListSectionItem<TabGroup> {
    return {
      title: [group.title],
      prefixIcon: {
        element: html`<tab-group-dot .color="${group.color}"
            .filled="${group.isOpen}"
            .size="${TabGroupDotSize.LARGE}"></tab-group-dot>`,
      },
      hoveredActionButton: {
        icon: 'cr:more-vert',
        ariaLabel: loadTimeData.getString('tabGroupMoreOptions'),
      },
      size: CrUrlListItemSize.COMPACT,
      data: group,
    };
  }
}
