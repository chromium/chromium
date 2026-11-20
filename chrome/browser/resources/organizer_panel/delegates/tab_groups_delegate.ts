// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/tab_group_shared/tab_group_dot.js';

import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {TemplateResult} from '//resources/lit/v3_0/lit.rollup.js';
import {html} from '//resources/lit/v3_0/lit.rollup.js';
import {TabGroupDotSize} from '/tab_group_shared/tab_group_dot.js';

import type {OrganizerAction, OrganizerListSectionClient, OrganizerListSectionDelegate} from '../organizer_list_section_delegate.js';
import type {OrganizerListSectionItem} from '../organizer_list_section_item.js';
import {OrganizerListSectionItemIconType} from '../organizer_list_section_item.js';
import {SectionId} from '../organizer_panel.mojom-webui.js';
import type {BrowserProxy, TabGroup} from '../tab_groups.mojom-webui.js';
import {browserProxyFactory} from '../tab_groups.mojom-webui.js';

import {getCompactZeroState, removeListeners} from './tab_delegate_utils.js';

export class TabGroupsDelegate implements
    OrganizerListSectionDelegate<TabGroup> {
  private browserProxy_: BrowserProxy = browserProxyFactory.getInstance();
  private client_?: OrganizerListSectionClient;
  private listenerIds_: number[] = [];
  private tabGroups_: TabGroup[] = [];
  private onCreateTabGroupClick_ = () => {
    chrome.metricsPrivate.recordUserAction('OrganizerPanel.CreateTabGroup');
    this.browserProxy_.handler.createNewTabGroup();
  };

  init(sectionClient: OrganizerListSectionClient) {
    assert(this.listenerIds_.length === 0);
    this.client_ = sectionClient;
    const callbackRouter = this.browserProxy_.callbackRouter;
    this.listenerIds_.push(
        callbackRouter.tabGroupAdded.addListener((tabGroup: TabGroup) => {
          this.onTabGroupAdded_(tabGroup);
        }),
        callbackRouter.tabGroupRemoved.addListener((id: string) => {
          this.onTabGroupRemoved_(id);
        }),
        callbackRouter.tabGroupUpdated.addListener((tabGroup: TabGroup) => {
          this.onTabGroupUpdated_(tabGroup);
        }));
  }

  destroy() {
    removeListeners(this.browserProxy_.callbackRouter, this.listenerIds_);
    this.client_ = undefined;
  }

  getId(): SectionId {
    return SectionId.kTabGroups;
  }

  getHeader(): string {
    return loadTimeData.getString('tabGroups');
  }

  async getItems(): Promise<Array<OrganizerListSectionItem<TabGroup>>> {
    const {tabGroups} = await this.browserProxy_.handler.getTabGroups();
    this.tabGroups_ = tabGroups;
    return this.tabGroups_.map(group => this.tabGroupToSectionItem_(group));
  }

  getZeroState(): TemplateResult {
    return getCompactZeroState(
        loadTimeData.getString('createTabGroup'), 'organizer-panel:library-add',
        this.onCreateTabGroupClick_);
  }

  shouldAlwaysShowZeroState(): boolean {
    return true;
  }

  onItemClick(item: OrganizerListSectionItem<TabGroup>) {
    chrome.metricsPrivate.recordUserAction('OrganizerPanel.OpenTabGroup');
    const data = item.data;
    assert(data);
    this.browserProxy_.handler.openTabGroup(data.id);
  }

  getItemClickAction(): OrganizerAction {
    return 'OpenTabGroup';
  }

  async onItemActionButtonClicked(
      item: OrganizerListSectionItem<TabGroup>, buttonElement: HTMLElement) {
    assert(item.data);
    const rect = buttonElement.getBoundingClientRect();
    await this.showContextMenu_(item.data.id, {
      x: Math.round(rect.x),
      y: Math.round(rect.y),
      width: Math.round(rect.width),
      height: Math.round(rect.height),
    });
  }

  async onItemContextMenuClicked(
      item: OrganizerListSectionItem<TabGroup>, x: number, y: number) {
    assert(item.data);
    await this.showContextMenu_(item.data.id, {
      x: Math.round(x),
      y: Math.round(y),
      width: 0,
      height: 0,
    });
  }

  private async showContextMenu_(
      id: string,
      anchorRect: {x: number, y: number, width: number, height: number}) {
    chrome.metricsPrivate.recordUserAction('OrganizerPanel.OpenTabGroupMenu');
    await this.browserProxy_.handler.showContextMenu(id, anchorRect);
  }

  private notifyClient_() {
    this.client_?.onItemsChanged(
        this.tabGroups_.map(group => this.tabGroupToSectionItem_(group)));
  }

  private onTabGroupAdded_(tabGroup: TabGroup) {
    this.tabGroups_ = [
      tabGroup,
      ...this.tabGroups_.filter(g => g.id !== tabGroup.id),
    ];
    this.notifyClient_();
  }

  private onTabGroupRemoved_(id: string) {
    this.tabGroups_ = this.tabGroups_.filter(g => g.id !== id);
    this.notifyClient_();
  }

  private onTabGroupUpdated_(tabGroup: TabGroup) {
    const index = this.tabGroups_.findIndex(g => g.id === tabGroup.id);
    if (index !== -1) {
      this.tabGroups_[index] = tabGroup;
      this.notifyClient_();
    }
  }

  private tabGroupToSectionItem_(group: TabGroup):
      OrganizerListSectionItem<TabGroup> {
    const hasTitle = Boolean(group.title.trim());
    const stateText = group.isOpen ? loadTimeData.getString('openTabGroup') :
                                     loadTimeData.getString('savedTabGroup');
    return {
      title: [group.title],
      ariaLabel: hasTitle ? undefined : [stateText],
      description: group.lastActiveElapsedText ?
          [{text: group.lastActiveElapsedText, matchable: false}] :
          undefined,
      ariaDescription: hasTitle ?
          (group.lastActiveElapsedText ?
               [stateText, group.lastActiveElapsedText] :
               [stateText]) :
          undefined,
      prefixIcon: {
        type: OrganizerListSectionItemIconType.ELEMENT,
        element: html`<tab-group-dot .color="${group.color}"
            .filled="${group.isOpen}"
            .size="${TabGroupDotSize.LARGE}"></tab-group-dot>`,
      },
      hoveredActionButton: {
        icon: 'cr:more-vert',
        ariaLabel: hasTitle ? loadTimeData.getStringF(
                                  'tabGroupMoreOptionsWithTitle', group.title) :
                              loadTimeData.getString('tabGroupMoreOptions'),
        tooltip: loadTimeData.getString('options'),
      },
      data: group,
    };
  }
}
