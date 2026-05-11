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
import type {Container, Tab, TabCreatedContainer, TabGroup, TabGroupVisualData} from '/tab_strip_api/tab_strip_api_data_model.mojom-webui.js';
import {OnDataChangedEventFieldTags, whichOnDataChangedEvent} from '/tab_strip_api/tab_strip_api_events.mojom-webui.js';
import type {OnCollectionCreatedEvent, OnDataChangedEvent, OnNodeMovedEvent, OnNodesClosedEvent, OnTabsCreatedEvent} from '/tab_strip_api/tab_strip_api_events.mojom-webui.js';
import type {NodeId} from '/tab_strip_api/tab_strip_api_types.mojom-webui.js';
import {TabStripObservation} from '/tab_strip_api/tab_strip_observation.js';
import type {TabStripObserver} from '/tab_strip_api/tab_strip_observer.js';

import type {TabActivated, TabAdded, TabClosed, TabUpdated} from './events.js';
import type {TabElement} from './tab.js';
import {getCss} from './tab_strip.css.js';
import {getHtml} from './tab_strip.html.js';

export interface TabItem {
  type: 'tab';
  id: string;
  tabData: Tab;
}

export interface TabGroupItem {
  type: 'group';
  id: string;
  groupData: TabGroupVisualData;
}

type TabStripItem = TabItem|TabGroupItem;

export interface TabStripElement {
  $: {
    tabstrip: HTMLElement,
  };
}

export class TabStripElement extends CrLitElement implements TabStripObserver {
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
    const tabStrip = tabSnapshot.tabStrip;
    const processContainer = (container: Container) => {
      if (!container || !container.children) {
        return;
      }
      container.children.forEach((containerElement: Container) => {
        if (containerElement.data.tab) {
          this.addTab(containerElement.data.tab);
        } else if (containerElement.data.tabGroup) {
          const group = containerElement.data.tabGroup;
          this.addGroup(group);
          processContainer(containerElement);
        } else {
          processContainer(containerElement);
        }
      });
    };
    processContainer(tabStrip);

    // Now initial state is processed, start listening to events.
    observation.bind(tabSnapshot.stream.handle);

    // The initial data load should contain at least one active tab.
    assert(this.activeTab_ !== '');
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

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (!changedProperties.has('activeTab_' as keyof TabStripElement)) {
      return;
    }

    for (let i = 0; i < this.items_.length; ++i) {
      const item = this.items_[i]!;
      const shouldBeActive = item.id === this.activeTab_;

      if (item.type !== 'tab' || item.tabData.isActive === shouldBeActive) {
        continue;
      }

      this.items_[i] = {
        ...item,
        tabData: {...item.tabData, isActive: shouldBeActive},
      };
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
        const tab = onDataChangedEvent.tab!.data;
        this.updateTab(tab);
        if (tab.isActive) {
          this.activeTab_ = tab.id;
        }
        break;
      case OnDataChangedEventFieldTags.TAB_GROUP:
        const tabGroup = onDataChangedEvent.tabGroup!.data;
        this.updateTabGroup(tabGroup);
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
    this.items_.push({
      type: 'tab',
      id: tab.id,
      tabData: tab,
    });
    this.requestUpdate();

    if (tab.isActive) {
      this.activeTab_ = tab.id;
    }

    this.fire<TabAdded>('tab-added', {
      id: tab.id,
      isActive: tab.isActive,
    });
  }

  private addGroup(group: TabGroup) {
    this.items_.push({
      type: 'group',
      id: group.id,
      groupData: group.data,
    });
    this.requestUpdate();
  }

  protected onTabCloseClick(e: CustomEvent<{id: string}>) {
    this.tabStripService_.closeNodes([e.detail.id]);
  }

  private activateTab(tabId: NodeId) {
    const item = this.items_.find(
        (i): i is TabItem => i.type === 'tab' && i.id === tabId);
    if (!item) {
      return;
    }

    this.activeTab_ = tabId;
    this.tabStripService_.activateTab(tabId);
    this.fire<TabActivated>('tab-activated', item.tabData);
  }

  // TODO(webium): implement this.
  // private setTabGroupForTab(/*tabId*/ _: NodeId, /*groupId*/ _2?: NodeId) {
  //}

  private updateTabGroup(group: TabGroup) {
    const index =
        this.items_.findIndex(i => i.type === 'group' && i.id === group.id);
    if (index !== -1) {
      const groupItem = this.items_[index] as TabGroupItem;
      this.items_[index] = {...groupItem, groupData: group.data};
      this.requestUpdate();
    }
  }

  private updateTab(tab: Tab) {
    const index =
        this.items_.findIndex(i => i.type === 'tab' && i.id === tab.id);
    if (index === -1) {
      return;
    }

    this.items_[index] = {
      type: 'tab',
      id: tab.id,
      tabData: tab,
    };
    // Needed to get the tab element to refresh with the updated data.
    this.requestUpdate();
    this.fire<TabUpdated>('tab-updated', tab);
  }

  private removeItem(id: NodeId) {
    const itemToRemove = this.items_.find(item => item.id === id);
    this.items_ = this.items_.filter(item => item.id !== id);
    this.requestUpdate();

    if (itemToRemove && itemToRemove.type === 'tab') {
      this.fire<TabClosed>('tab-closed', id);
    }
  }

  protected onNewTabButtonClick_() {
    this.tabStripService_.createTabAt(null, null);
  }

  // Tab IDs contain ":" characters which are not valid when used as DOM node
  // IDs, and also an ID must contain at least one non-special and non-numeric
  // character.
  protected tabIdToDomId(id: NodeId): string {
    return 'id_' + id.replaceAll(':', '_');
  }

  // TODO(webium): Move drag logic out into its own session object or
  // controller.

  // Drag experience variables.
  private lastMouseEvent: MouseEvent|null = null;
  // The initial relative position of the left edge of the dragged element to
  // the cursor. This is used to maintain the relative positioing throughout
  // the drag session.
  private mouseXOffset = 0;

  // Out of bounds dragOffset.
  private outOfBoundsDragX = 0;
  private outOfBoundsDragY = 0;

  // Set during drag events.
  private draggedTabId = '';

  dragMouseDown(e: MouseEvent) {
    e = e || window.event;
    e.preventDefault();
    const path = e.composedPath();
    const tabElement =
        path.find(
            el => el instanceof Element &&
                el.localName === 'webui-browser-tab') as TabElement |
        null;
    if (tabElement) {
      this.draggedTabId = tabElement.tabData.id;
      this.dragInProgress_ = true;
      this.outOfBoundsDragX = e.clientX;
      this.outOfBoundsDragY = e.clientY;
      this.$.tabstrip.classList.add('nodrag');
      this.activateTab(this.draggedTabId);
      this.lastMouseEvent = e;
      this.mouseXOffset = e.clientX - tabElement.getBoundingClientRect().left;
    }
  }

  closeDragElement() {
    if (!this.dragInProgress_) {
      return;
    }

    this.lastMouseEvent = null;

    this.getDraggedElement().style.transform = '';
    this.draggedTabId = '';
    this.mouseXOffset = 0;
    this.lastMouseEvent = null;

    this.$.tabstrip.classList.remove('nodrag');
    this.dragInProgress_ = false;
  }

  // This is necessary to recompute the position of the dragged element
  // relative to the cursor before a repaint. If we do not do this, users may
  // observe a flicker when slowly dragging the element.
  override update(changedProperties: PropertyValues<this>) {
    super.update(changedProperties);
    if (this.dragInProgress_) {
      assert(this.lastMouseEvent);
      for (const element of this.shadowRoot.querySelectorAll(
               'webui-browser-tab')) {
        // Containers may be reused and rebound to different data during drag.
        // We need to reset the position for all tab elements and retarget the
        // container holding the tab that's being dragged.
        element.style.transform = '';
      }
      this.moveElementToCursor(this.lastMouseEvent.clientX);
    }
  }

  private getDraggedElement(): TabElement {
    assert(this.dragInProgress_ && this.draggedTabId);
    const element = this.shadowRoot.querySelector<TabElement>(
        `webui-browser-tab#${this.tabIdToDomId(this.draggedTabId)}`);
    assert(element);
    return element;
  }

  elementDrag(e: MouseEvent) {
    e = e || window.event;
    e.preventDefault();
    if (!this.dragInProgress_) {
      return;
    }
    this.lastMouseEvent = e;

    // Move the tab to its new position relative to the current cursor position.
    this.moveElementToCursor(e.clientX);
    const dragElementRect = this.getDraggedElement().getBoundingClientRect();

    // Now we will test the positioning after the DOM has been laid out.
    const index = this.items_.findIndex(item => {
      return item.type === 'tab' && item.id === this.draggedTabId;
    });
    assert(index !== -1);
    // Test for swap forward case.
    const prevItem = this.items_[index - 1];
    if (prevItem && prevItem.type === 'tab') {
      const targetIdx = index - 1;
      const targetId = this.tabIdToDomId(prevItem.id);
      const target = this.shadowRoot.querySelector<TabElement>(
          `webui-browser-tab#${targetId}`);
      assert(target);
      const targetMidpoint = target.getBoundingClientRect().left +
          (target.getBoundingClientRect().width / 2);
      if (dragElementRect.left < targetMidpoint) {
        [this.items_[index], this.items_[targetIdx]] =
            [this.items_[targetIdx]!, this.items_[index]!];
        this.items_ = [...this.items_];
      }
    }
    // Test for swap backward case.
    const nextItem = this.items_[index + 1];
    if (nextItem && nextItem.type === 'tab') {
      const targetIdx = index + 1;
      const targetId = this.tabIdToDomId(nextItem.id);
      const target = this.shadowRoot.querySelector<TabElement>(
          `webui-browser-tab#${targetId}`);
      assert(target);
      const targetMidpoint = target.getBoundingClientRect().left +
          (target.getBoundingClientRect().width / 2);
      if (dragElementRect.right > targetMidpoint) {
        [this.items_[index], this.items_[targetIdx]] =
            [this.items_[targetIdx]!, this.items_[index]!];
        this.items_ = [...this.items_];
      }
    }

    // Check if tab is being dragged outside of bounds +/- artificial margins.
    if (e.clientX < this.getBoundingClientRect().left ||
        e.clientX >= this.getBoundingClientRect().right - 1 ||
        e.clientY < this.getBoundingClientRect().top ||
        e.clientY > this.getBoundingClientRect().bottom + 10) {
      this.outOfBoundsHandler(this.draggedTabId);
    }
  }

  // Moves the dragged element to its relative position to the cursor at the
  // start of the drag.
  private moveElementToCursor(mouseClientX: number) {
    assert(this.dragInProgress_);

    const tabElement = this.getDraggedElement();
    // Using the mouse cursor as a frame of reference, compute where the tab
    // element needs to be to maintain the same relative position at the
    // start of the drag.
    // First we reset the transformation so we know where the element should
    // be.
    tabElement.style.transform = '';
    const deltaX = mouseClientX - tabElement.getBoundingClientRect().left -
        this.mouseXOffset;
    tabElement.style.transform = `translateX(${deltaX}px)`;
  }

  private outOfBoundsHandler(tabId: NodeId) {
    this.fire('tab-drag-out-of-bounds', {
      tabId: tabId,
      drag_offset_x: this.outOfBoundsDragX,
      drag_offset_y: this.outOfBoundsDragY,
    });
    this.closeDragElement();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'webui-browser-tab-strip': TabStripElement;
  }
}

customElements.define(TabStripElement.is, TabStripElement);
