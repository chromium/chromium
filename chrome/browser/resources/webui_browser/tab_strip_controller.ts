// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TabStripService} from '/tab_strip_api/tab_strip_api.mojom-webui.js';
import type {TabStripServiceRemote} from '/tab_strip_api/tab_strip_api.mojom-webui.js';
import type {Container, Tab, TabCreatedContainer} from '/tab_strip_api/tab_strip_api_data_model.mojom-webui.js';
import type {OnTabDataChangedEvent, OnTabGroupVisualsChangedEvent, OnTabsClosedEvent, OnTabsCreatedEvent} from '/tab_strip_api/tab_strip_api_events.mojom-webui.js';
import type {NodeId} from '/tab_strip_api/tab_strip_api_types.mojom-webui.js';
import {TabStripObservation} from '/tab_strip_api/tab_strip_observation.js';

import type {ContentRegion} from './content_region.js';
import type {TabStrip} from './tab_strip.js';

export interface LayoutManager {
  // Notifies the layout manager to recompute its layout, because the tab strip
  // might have changed.
  refreshLayout: () => void;
}

export class TabStripController {
  private readonly layoutManager_: LayoutManager;
  private readonly tabStripService_: TabStripServiceRemote;
  private readonly tabStripObservation_: TabStripObservation;
  private tabStrip_: TabStrip;
  private contentRegion_: ContentRegion;

  constructor(
      layoutManager: LayoutManager, tabStrip: TabStrip,
      contentRegion: ContentRegion) {
    this.layoutManager_ = layoutManager;
    this.tabStripService_ = TabStripService.getRemote();
    this.tabStripObservation_ = new TabStripObservation();
    this.tabStrip_ = tabStrip;
    this.contentRegion_ = contentRegion;

    this.registerTabChangeCallbacks_();
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
  private registerTabChangeCallbacks_() {
    // TODO(webium): implement these callbacks.
    // this.tabStripObservation_.showContextMenu.addListener(
    //     () => this.onShowContextMenu_());
    this.tabStripObservation_.onTabsCreated.addListener(
        this.onTabsCreated_.bind(this));
    // this.tabStripObservation_.tabMoved.addListener(
    //    this.onTabMoved_.bind(this));
    this.tabStripObservation_.onTabsClosed.addListener(
        this.onTabsClosed_.bind(this));
    this.tabStripObservation_.onTabDataChanged.addListener(
        this.onTabDataChanged_.bind(this));
    // this.tabStripObservation_.tabReplaced.addListener(
    //    this.onTabReplaced_.bind(this));
    // this.tabStripObservation_.tabCloseCancelled.addListener(
    //     this.onTabCloseCancelled_.bind(this));
    // this.tabStripObservation_.tabGroupStateChanged.addListener(
    //    this.onTabGroupStateChanged_.bind(this));
    // this.tabStripObservation_.tabGroupClosed.addListener(
    //     this.onTabGroupClosed_.bind(this));
    // this.tabStripObservation_.tabGroupMoved.addListener(
    //     this.onTabGroupMoved_.bind(this));
    this.tabStripObservation_.onTabGroupVisualsChanged.addListener(
        this.onTabGroupVisualsChanged_.bind(this));
  }

  private async loadTabStripModel_() {
    const tabSnapshot = await this.tabStripService_.getTabs();
    // TODO(crbug.com/439844342): add type signature.
    this.tabStripObservation_.bind((tabSnapshot.stream as any).handle);

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
  }

  private addTab_(tab: Tab) {
    this.tabStrip_.addTab(tab);
    if (tab.isActive) {
      this.tabStrip_.activateTab(tab.id);
    }
    this.contentRegion_.createWebView(tab.id, tab.isActive);
    this.layoutManager_.refreshLayout();
  }

  // tab_strip::mojom::Page implementation:
  private onTabsCreated_(tabsCreatedEvent: OnTabsCreatedEvent) {
    const tabsCreated: TabCreatedContainer[] = tabsCreatedEvent.tabs;
    tabsCreated.forEach((container) => {
      this.addTab_(container.tab);
    });
  }

  /* TODO(webium): get this working.
  private onTabGroupStateChanged_(tabId: NodeId, _: number, groupId?: NodeId) {
    this.tabStrip_.setTabGroupForTab_(tabId, groupId);
  }
  */

  private onTabDataChanged_(onTabDataChangedEvent: OnTabDataChangedEvent) {
    this.tabStrip_.updateTab(onTabDataChangedEvent.tab);
    if (onTabDataChangedEvent.tab.isActive) {
      this.tabStrip_.activateTab(onTabDataChangedEvent.tab.id);
      this.contentRegion_.activateTab(onTabDataChangedEvent.tab.id);
    }
    this.layoutManager_.refreshLayout();
  }

  private onTabGroupVisualsChanged_(event: OnTabGroupVisualsChangedEvent) {
    const {tabGroup} = event.data;
    if (tabGroup) {
      this.tabStrip_.setTabGroupVisualData(tabGroup.id, tabGroup.data);
    }
  }

  private onTabsClosed_(tabsClosedEvent: OnTabsClosedEvent) {
    const tabsClosed = tabsClosedEvent.tabs;
    tabsClosed.forEach((tabId: NodeId) => {
      this.tabStrip_.removeTab(tabId);
      this.contentRegion_.removeTab(tabId);
    });
  }
}
