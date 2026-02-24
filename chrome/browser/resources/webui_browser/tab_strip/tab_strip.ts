// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './tab.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';

import {assert, assertNotReachedCase} from '//resources/js/assert.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {TabStripService} from '/tab_strip_api/tab_strip_api.mojom-webui.js';
import type {TabStripServiceRemote} from '/tab_strip_api/tab_strip_api.mojom-webui.js';
import type {Container, Tab as TabData, TabCreatedContainer, TabGroupVisualData} from '/tab_strip_api/tab_strip_api_data_model.mojom-webui.js';
import {DataFieldTags, whichData} from '/tab_strip_api/tab_strip_api_data_model.mojom-webui.js';
import type {OnCollectionCreatedEvent, OnDataChangedEvent, OnNodeMovedEvent, OnTabsClosedEvent, OnTabsCreatedEvent} from '/tab_strip_api/tab_strip_api_events.mojom-webui.js';
import type {NodeId} from '/tab_strip_api/tab_strip_api_types.mojom-webui.js';
import {TabStripObservation} from '/tab_strip_api/tab_strip_observation.js';
import type {TabStripObserver} from '/tab_strip_api/tab_strip_observer.js';

import type {TabActivated, TabAdded, TabClosed, TabUpdated} from './events.js';
import type {TabElement} from './tab.js';
import {getCss} from './tab_strip.css.js';
import {getHtml} from './tab_strip.html.js';

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
      tabs_: {
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

  protected accessor tabs_: TabData[] = [];
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
      container.children.forEach((containerElement: Container, _: number) => {
        if (containerElement.data.tab) {
          this.addTab(containerElement.data.tab);
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

  onTabsClosed(tabsClosedEvent: OnTabsClosedEvent) {
    tabsClosedEvent.tabs.forEach(tabId => {
      this.removeTab(tabId);
    });
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    for (let i = 0; i < this.tabs_.length; ++i) {
      const tab = this.tabs_[i]!;
      const tabShouldBeActive = tab.id === this.activeTab_;
      const activationChanged = tab.isActive !== tabShouldBeActive;
      if (activationChanged) {
        this.tabs_[i] = {...tab, isActive: tabShouldBeActive};
      }
    }
  }

  /* TODO(webium): get this working.
  private onTabGroupStateChanged_(tabId: NodeId, _: number, groupId?: NodeId) {
    this.tabStrip_.setTabGroupForTab_(tabId, groupId);
  }
  */

  onDataChanged(onDataChangedEvent: OnDataChangedEvent) {
    const data = onDataChangedEvent.data;
    const tag = whichData(data);
    switch (tag) {
      case DataFieldTags.TAB:
        const tab = data.tab!;
        this.updateTab(tab);
        if (tab.isActive) {
          this.activeTab_ = tab.id;
        }
        break;
      case DataFieldTags.TAB_GROUP:
        const tabGroup = data.tabGroup!;
        this.setTabGroupVisualData(tabGroup.id, tabGroup.data);
        break;
      case DataFieldTags.TAB_STRIP:
      case DataFieldTags.PINNED_TABS:
      case DataFieldTags.UNPINNED_TABS:
      case DataFieldTags.TAB_GROUP:
      case DataFieldTags.SPLIT_TAB:
      case DataFieldTags.WINDOW:
        throw new Error(`unimplemented type: ${data}`);
      default:
        assertNotReachedCase(tag);
    }
  }

  onCollectionCreated(_onCollectionCreated: OnCollectionCreatedEvent) {}

  onNodeMoved(_onNodeMoved: OnNodeMovedEvent) {}

  // End TabStripObserver impl:

  private addTab(tab: TabData) {
    this.tabs_.push(tab);
    this.requestUpdate();
    if (tab.isActive) {
      this.activeTab_ = tab.id;
    }

    this.fire<TabAdded>('tab-added', {
      id: tab.id,
      isActive: tab.isActive,
    });
  }

  protected onTabCloseClick(e: CustomEvent) {
    this.tabStripService_.closeTabs([e.detail.id]);
  }

  private activateTab(tabId: NodeId) {
    const targetActive = this.tabs_.find(tab => tab.id === tabId);
    if (!targetActive) {
      return;
    }

    this.activeTab_ = tabId;
    targetActive.isActive = true;
    this.tabStripService_.activateTab(tabId);
    this.requestUpdate();
    this.fire<TabActivated>('tab-activated', targetActive);
  }

  // TODO(webium): implement this.
  // private setTabGroupForTab(/*tabId*/ _: NodeId, /*groupId*/ _2?: NodeId) {
  //}

  private setTabGroupVisualData(_: string, _2: TabGroupVisualData) {
    // TODO(webium): implement this.
  }

  private updateTab(tabData: TabData) {
    const targetIdx = this.tabs_.findIndex(tab => tab.id === tabData.id);
    if (targetIdx === -1) {
      return;
    }

    this.tabs_[targetIdx] = tabData;
    // Needed to get the tab element to refresh with the updated data.
    this.requestUpdate();
    this.fire<TabUpdated>('tab-updated', tabData);
  }

  private removeTab(tabId: NodeId) {
    this.tabs_ = this.tabs_.filter(tab => tab.id !== tabId);
    this.fire<TabClosed>('tab-closed', tabId);
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
      this.draggedTabId = tabElement.data.id;
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
    const index = this.tabs_.findIndex(tab => {
      return tab.id === this.draggedTabId;
    });
    assert(index !== -1);
    // Test for swap forward case.
    if (this.tabs_[index - 1]) {
      const targetIdx = index - 1;
      const targetId = this.tabIdToDomId(this.tabs_[targetIdx]!.id);
      const target = this.shadowRoot.querySelector<TabElement>(
          `webui-browser-tab#${targetId}`);
      assert(target);
      const targetMidpoint = target.getBoundingClientRect().left +
          (target.getBoundingClientRect().width / 2);
      if (dragElementRect.left < targetMidpoint) {
        [this.tabs_[index], this.tabs_[targetIdx]] =
            [this.tabs_[targetIdx]!, this.tabs_[index]!];
        this.tabs_ = [...this.tabs_];
      }
    }
    // Test for swap backward case.
    if (this.tabs_[index + 1]) {
      const targetIdx = index + 1;
      const targetId = this.tabIdToDomId(this.tabs_[targetIdx]!.id);
      const target = this.shadowRoot.querySelector<TabElement>(
          `webui-browser-tab#${targetId}`);
      assert(target);
      const targetMidpoint = target.getBoundingClientRect().left +
          (target.getBoundingClientRect().width / 2);
      if (dragElementRect.right > targetMidpoint) {
        [this.tabs_[index], this.tabs_[targetIdx]] =
            [this.tabs_[targetIdx]!, this.tabs_[index]!];
        this.tabs_ = [...this.tabs_];
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
