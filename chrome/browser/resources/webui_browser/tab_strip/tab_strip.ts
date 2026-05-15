// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './tab.js';
import './tab_group.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';

import {assert, assertNotReachedCase} from '//resources/js/assert.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {TabStripService} from '/tab_strip_api/tab_strip_api.mojom-webui.js';
import type {TabStripServiceRemote} from '/tab_strip_api/tab_strip_api.mojom-webui.js';
import type {Container, Tab, TabCreatedContainer, TabGroup} from '/tab_strip_api/tab_strip_api_data_model.mojom-webui.js';
import {OnDataChangedEventFieldTags, whichOnDataChangedEvent} from '/tab_strip_api/tab_strip_api_events.mojom-webui.js';
import type {OnCollectionCreatedEvent, OnDataChangedEvent, OnNodeMovedEvent, OnNodesClosedEvent, OnTabsCreatedEvent} from '/tab_strip_api/tab_strip_api_events.mojom-webui.js';
import type {NodeId} from '/tab_strip_api/tab_strip_api_types.mojom-webui.js';
import {TabStripObservation} from '/tab_strip_api/tab_strip_observation.js';
import type {TabStripObserver} from '/tab_strip_api/tab_strip_observer.js';

import type {TabActivated, TabAdded, TabClosed, TabUpdated} from './events.js';
import type {TabGroupItem, TabItem, TabStripItem} from './items.js';
import type {TabElement} from './tab.js';
import {TabDragDelegate} from './tab_drag_delegate.js';
import type {TabDragHost} from './tab_drag_host.js';
import {getCss} from './tab_strip.css.js';
import {getHtml} from './tab_strip.html.js';

export type {TabGroupItem, TabItem, TabStripItem};

export interface TabStripElement {
  $: {
    tabstrip: HTMLElement,
  };
}

export class TabStripElement extends CrLitElement implements TabStripObserver,
                                                             TabDragHost {
  static get is() {
    return 'webui-browser-tab-strip';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      items_: {
        type: Array,
      },
      activeTab_: {
        type: String,
        state: true,
      },
      dragInProgress_: {
        type: Boolean,
        state: true,
      },
    };
  }

  protected accessor items_: TabStripItem[] = [];
  protected accessor activeTab_: string = '';
  protected accessor dragInProgress_ = false;

  private readonly tabStripService_: TabStripServiceRemote;
  private tabStripObservation_: TabStripObservation|undefined;
  private dragDelegate_ = new TabDragDelegate(this);

  constructor() {
    super();

    this.tabStripService_ = TabStripService.getRemote();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.tabStripObservation_ = new TabStripObservation(this);
    this.loadTabStripModel_(this.tabStripObservation_);
  }

  private async loadTabStripModel_(observation: TabStripObservation) {
    const tabSnapshot = await this.tabStripService_.getTabs();
    this.processModelContainer_(tabSnapshot.tabStrip);

    // Now initial state is processed, start listening to events.
    observation.bind(tabSnapshot.stream.handle);

    // The initial data load should contain at least one active tab.
    assert(this.activeTab_ !== '', 'initial snapshot contains no active tab');
  }

  private processModelContainer_(container: Container) {
    if (!container || !container.children) {
      return;
    }
    container.children.forEach((containerElement: Container) => {
      if (containerElement.data.tab) {
        this.addTab(containerElement.data.tab);
      } else if (containerElement.data.tabGroup) {
        const group = containerElement.data.tabGroup;
        this.addGroup(group);
        this.processModelContainer_(containerElement);
      } else {
        this.processModelContainer_(containerElement);
      }
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.tabStripObservation_ = undefined;
  }

  // TabStripObserver impl:
  onTabsCreated(tabsCreatedEvent: OnTabsCreatedEvent) {
    const tabsCreated: TabCreatedContainer[] = tabsCreatedEvent.tabs;
    tabsCreated.forEach(container => {
      this.addTab(container.tab);
    });
  }

  onNodesClosed(nodesClosedEvent: OnNodesClosedEvent) {
    nodesClosedEvent.nodeIds.forEach(nodeId => {
      this.removeItem(nodeId);
    });
  }

  override update(changedProperties: PropertyValues<this>) {
    super.update(changedProperties);
    this.dragDelegate_.onUpdate();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('activeTab_' as keyof TabStripElement)) {
      this.syncTabActiveStates_();
    }
  }

  private syncTabActiveStates_() {
    let hasChanges = false;
    const updatedItems = [...this.items_];
    for (let i = 0; i < updatedItems.length; ++i) {
      const item = updatedItems[i]!;
      const shouldBeActive = item.id === this.activeTab_;

      if (item.type !== 'tab' || item.tabData.isActive === shouldBeActive) {
        continue;
      }

      updatedItems[i] = {
        ...item,
        tabData: {...item.tabData, isActive: shouldBeActive},
      };
      hasChanges = true;
    }

    if (hasChanges) {
      this.setItems_(updatedItems);
    }
  }

  /* TODO(webium): get this working.
  private onTabGroupStateChanged_(tabId: NodeId, _: number, groupId?: NodeId) {
    this.tabStrip_.setTabGroupForTab_(tabId, groupId);
  }
  */

  onDataChanged(onDataChangedEvent: OnDataChangedEvent) {
    const tag = whichOnDataChangedEvent(onDataChangedEvent);
    switch (tag) {
      case OnDataChangedEventFieldTags.TAB:
        this.updateTab(onDataChangedEvent.tab!.data);
        break;
      case OnDataChangedEventFieldTags.TAB_GROUP:
        this.updateTabGroup(onDataChangedEvent.tabGroup!.data);
        break;
      case OnDataChangedEventFieldTags.SPLIT_TAB:
        throw new Error('unimplemented type: splitTab');
      default:
        assertNotReachedCase(tag);
    }
  }

  onCollectionCreated(_onCollectionCreated: OnCollectionCreatedEvent) {}

  onNodeMoved(_onNodeMoved: OnNodeMovedEvent) {}

  // End TabStripObserver impl:

  private addTab(tab: Tab) {
    this.setItems_([
      ...this.items_,
      {
        type: 'tab',
        id: tab.id,
        tabData: tab,
      },
    ]);

    if (tab.isActive) {
      this.setActiveTab_(tab.id);
    }

    this.fire<TabAdded>('tab-added', {
      id: tab.id,
      isActive: tab.isActive,
    });
  }

  private addGroup(group: TabGroup) {
    this.setItems_([
      ...this.items_,
      {
        type: 'group',
        id: group.id,
        groupData: group.data,
      },
    ]);
  }

  protected onTabCloseClick(e: CustomEvent<{id: string}>) {
    this.tabStripService_.closeNodes([e.detail.id]);
  }

  protected activateTab(tabId: NodeId) {
    const item = this.findItem_<TabItem>(tabId, 'tab');
    if (!item) {
      return;
    }

    this.setActiveTab_(tabId);
    this.tabStripService_.activateTab(tabId);
    this.fire<TabActivated>('tab-activated', item.tabData);
  }

  // TODO(webium): implement this.
  // private setTabGroupForTab(/*tabId*/ _: NodeId, /*groupId*/ _2?: NodeId) {
  //}

  private updateTabGroup(group: TabGroup) {
    const groupItem = this.findItem_<TabGroupItem>(group.id, 'group');
    if (groupItem) {
      this.updateItem_(
          group.id, 'group', {...groupItem, groupData: group.data});
    }
  }

  private updateTab(tab: Tab) {
    const updated = this.updateItem_(tab.id, 'tab', {
      type: 'tab',
      id: tab.id,
      tabData: tab,
    });

    if (updated) {
      if (tab.isActive) {
        this.setActiveTab_(tab.id);
      }
      this.fire<TabUpdated>('tab-updated', tab);
    }
  }

  private removeItem(id: NodeId) {
    const itemToRemove = this.findItem_<TabStripItem>(id);
    this.setItems_(this.items_.filter(item => item.id !== id));

    if (itemToRemove && itemToRemove.type === 'tab') {
      this.fire<TabClosed>('tab-closed', id);
    }
  }

  protected onNewTabButtonClick_() {
    this.tabStripService_.createTabAt(null, null);
  }

  // Utility helpers:
  private findItemIndex_(id: string, type?: 'tab'|'group'): number {
    return this.items_.findIndex(
        item => item.id === id && (!type || item.type === type));
  }

  private findItem_<T extends TabStripItem>(id: string, type?: T['type']): T
      |undefined {
    return this.items_.find(
               item => item.id === id && (!type || item.type === type)) as T |
        undefined;
  }

  protected getTabElement_(id: string): TabElement|null {
    return this.shadowRoot.querySelector<TabElement>(
        `webui-browser-tab#${this.tabIdToDomId(id)}`);
  }

  private setActiveTab_(id: string) {
    this.activeTab_ = id;
  }

  protected setItems_(items: TabStripItem[]) {
    this.items_ = items;
  }

  private updateItemAt_(index: number, item: TabStripItem) {
    const newItems = [...this.items_];
    newItems[index] = item;
    this.setItems_(newItems);
  }

  private updateItem_(id: string, type: 'tab'|'group', newItem: TabStripItem):
      boolean {
    const index = this.findItemIndex_(id, type);
    if (index === -1) {
      return false;
    }

    this.updateItemAt_(index, newItem);
    return true;
  }

  // Tab IDs contain ":" characters which are not valid when used as DOM node
  // IDs, and also an ID must contain at least one non-special and non-numeric
  // character.
  protected tabIdToDomId(id: NodeId): string {
    return 'id_' + id.replaceAll(':', '_');
  }

  // TabDragHost impl:

  get itemsForDrag() {
    return this.items_;
  }

  getTabElementForDrag(id: string) {
    return this.getTabElement_(id);
  }

  setItemsForDrag(items: TabStripItem[]) {
    this.setItems_(items);
  }

  activateTabForDrag(id: string) {
    this.activateTab(id);
  }

  setDragInProgressForDrag(value: boolean) {
    this.dragInProgress_ = value;
  }

  setTabStripNoDrag(noDrag: boolean) {
    this.$.tabstrip.classList.toggle('nodrag', noDrag);
  }

  dragMouseDown(e: MouseEvent) {
    this.dragDelegate_.onMouseDown(e);
  }

  elementDrag(e: MouseEvent) {
    this.dragDelegate_.onMouseMove(e);
  }

  closeDragElement() {
    this.dragDelegate_.onMouseUp();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'webui-browser-tab-strip': TabStripElement;
  }
}

customElements.define(TabStripElement.is, TabStripElement);
