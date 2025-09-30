// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TabStripService} from '/tab_strip_api/tab_strip_api.mojom-webui.js';
import type {TabStripServiceRemote} from '/tab_strip_api/tab_strip_api.mojom-webui.js';
import type {Container, Tab, TabCreatedContainer} from '/tab_strip_api/tab_strip_api_data_model.mojom-webui.js';
import type {OnCollectionCreatedEvent, OnDataChangedEvent, OnNodeMovedEvent, OnTabsClosedEvent, OnTabsCreatedEvent} from '/tab_strip_api/tab_strip_api_events.mojom-webui.js';
import type {NodeId} from '/tab_strip_api/tab_strip_api_types.mojom-webui.js';
import {TabStripObservation} from '/tab_strip_api/tab_strip_observation.js';
import type {TabStripObserver} from '/tab_strip_api/tab_strip_observer.js';

import type {ContentRegion} from './content_region.js';
import type {TabStrip} from './tab_strip.js';

export interface TabStripControllerDelegate {
  // Notifies the layout manager to recompute its layout, because the tab strip
  // might have changed.
  refreshLayout: () => void;

  // Notifies that the active tab has updated.
  activeTabUpdated: (tabData: Tab) => void;
}

export class TabStripController implements TabStripObserver {
  private readonly tabStripControllerDelegate_: TabStripControllerDelegate;
  private readonly tabStripService_: TabStripServiceRemote;
  private readonly tabStripObservation_: TabStripObservation;
  private tabStrip_: TabStrip;
  private contentRegion_: ContentRegion;

  constructor(
      tabStripControllerDelegate: TabStripControllerDelegate,
      tabStrip: TabStrip, contentRegion: ContentRegion) {
    this.tabStripControllerDelegate_ = tabStripControllerDelegate;
    this.tabStripService_ = TabStripService.getRemote();
    this.tabStripObservation_ = new TabStripObservation(this);
    this.tabStrip_ = tabStrip;
    this.contentRegion_ = contentRegion;

    this.loadTabStripModel_();
  }

  addNewTab() {
    // Asynchronously the browser will call onTabCreated_() and
    // onTabActivated_().
    this.tabStripService_.createTabAt(null, null);
  }

  removeTab(tabId: NodeId) {
    // Asynchronously the browser will call onTabRemoved_().
    this.tabStripService_.closeTabs([tabId]);
  }

  /* TODO(webium): Do we need this? if so, rewrite to use getTabs().
  public async getGroupVisualData_(groupId: NodeId):
  Promise<TabGroupVisualData|undefined> { const allVisualData = await
  this.tabStripService_.getGroupVisualData(); return
  allVisualData.data[groupId];
  }
  */

  onTabClick(e: CustomEvent) {
    this.tabStripService_.activateTab(e.detail.tabId);
  }

  onTabDragOutOfBounds(_: CustomEvent) {
    /* TODO(webium): Implement this.
    const tabId = e.detail.tabId;
    const dragOffsetX = e.detail.drag_offset_x;
    const dragOffsetY = e.detail.drag_offset_y;
    this.tabStripService_.detachTab(tabId, dragOffsetX, dragOffsetY);
    */
  }

  // Private methods:

  private async loadTabStripModel_() {
    const tabSnapshot = await this.tabStripService_.getTabs();
    const tabStrip = tabSnapshot.tabStrip;
    const processContainer = (container: Container) => {
      if (!container || !container.children) {
        return;
      }
      container.children.forEach((containerElement: Container, _: number) => {
        if (containerElement.data.tab) {
          this.addTab_(containerElement.data.tab);
        } else {
          processContainer(containerElement);
        }
      });
    };
    processContainer(tabStrip);

    // Now initial state is processed, start listening to events.
    this.tabStripObservation_.bind(tabSnapshot.stream.handle);
  }

  private addTab_(tab: Tab) {
    this.tabStrip_.addTab(tab);
    if (tab.isActive) {
      this.tabStrip_.activateTab(tab.id);
    }
    this.contentRegion_.createWebView(tab.id, tab.isActive);
    this.tabStripControllerDelegate_.refreshLayout();
  }

  // TabStripObserver impl:
  onTabsCreated(tabsCreatedEvent: OnTabsCreatedEvent) {
    const tabsCreated: TabCreatedContainer[] = tabsCreatedEvent.tabs;
    tabsCreated.forEach((container) => {
      this.addTab_(container.tab);
    });
  }

  onTabsClosed(tabsClosedEvent: OnTabsClosedEvent) {
    const tabsClosed = tabsClosedEvent.tabs;
    tabsClosed.forEach((tabId: NodeId) => {
      this.tabStrip_.removeTab(tabId);
      this.contentRegion_.removeTab(tabId);
    });
  }
  /* TODO(webium): get this working.
  private onTabGroupStateChanged_(tabId: NodeId, _: number, groupId?: NodeId) {
    this.tabStrip_.setTabGroupForTab_(tabId, groupId);
  }
  */

  onDataChanged(onDataChangedEvent: OnDataChangedEvent) {
    const data = onDataChangedEvent.data;
    if (data.tab) {
      const tab = data.tab;
      this.tabStrip_.updateTab(tab);
      if (tab.isActive) {
        this.tabStrip_.activateTab(tab.id);
        this.contentRegion_.activateTab(tab.id);
        this.tabStripControllerDelegate_.activeTabUpdated(tab);
      }
      this.tabStripControllerDelegate_.refreshLayout();
    } else if (data.tabGroup) {
      const tabGroup = data.tabGroup;
      if (tabGroup) {
        this.tabStrip_.setTabGroupVisualData(tabGroup.id, tabGroup.data);
      }
    }
  }

  onCollectionCreated(_onCollectionCreated: OnCollectionCreatedEvent) {}

  onNodeMoved(_onNodeMoved: OnNodeMovedEvent) {}
}
