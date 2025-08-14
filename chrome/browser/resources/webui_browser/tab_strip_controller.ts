// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(webium): Once ContentRegion is implemented:
// import {ContentRegion} from './content_region.js';
import type {TabStrip} from './tab_strip.js';
import type {TabStripApiProxy} from './tab_strip_api.js';
import {TabStripApiProxyImpl} from './tab_strip_api.js';
import type {TabsSnapshot} from './tab_strip_api.mojom-webui.js';
import type {Container, Tab, TabCreatedContainer} from './tab_strip_api_data_model.mojom-webui.js';
import type {OnTabDataChangedEvent, OnTabGroupVisualsChangedEvent, OnTabsClosedEvent, OnTabsCreatedEvent} from './tab_strip_api_events.mojom-webui.js';
import type {NodeId} from './tab_strip_api_types.mojom-webui.js';

export interface LayoutManager {
  // Notifies the layout manager to recompute its layout, because the tab strip
  // might have changed.
  refreshLayout: () => void;
}

export class TabStripController {
  private readonly layoutManager_: LayoutManager;
  private tabsApi_: TabStripApiProxy;

  // @ts-expect-error: initialized in init_().
  private tabStrip_: TabStrip;
  // TODO(webium): Once ContentRegion is implemented:
  // // @ts-ignore: initialized in init_().
  // private contentRegion_: ContentRegion;

  constructor(layoutManager: LayoutManager) {
    this.layoutManager_ = layoutManager;
    this.tabsApi_ = TabStripApiProxyImpl.getInstance();
  }

  init(
      tabStrip: TabStrip,
      // TODO(webium): Once ContentRegion is implemented:
      // contentRegion: ContentRegion
  ) {
    tabStrip.controller = this;
    this.tabStrip_ = tabStrip;
    // TODO(webium): Once ContentRegion is implemented:
    // this.contentRegion_ = contentRegion;

    this.registerTabChangeCallbacks_();
    this.loadTabStripModel_();
  }

  addNewTab() {
    // Asynchronously the browser will call onTabCreated_() and
    // onTabActivated_().
    this.tabsApi_.createTabAt(null, null);
  }

  removeTab(tabId: NodeId) {
    // Asynchronously the browser will call onTabRemoved_().
    this.tabsApi_.closeTabs([tabId]);
  }

  /* TODO(webium): Do we need this? if so, rewrite to use getTabs().
  public async getGroupVisualData_(groupId: NodeId):
  Promise<TabGroupVisualData|undefined> { const allVisualData = await
  this.tabsApi_.getGroupVisualData(); return allVisualData.data[groupId];
  }
  */

  onTabClick(e: CustomEvent) {
    this.tabsApi_.activateTab(e.detail.tabId);
  }

  onTabDragOutOfBounds(_: CustomEvent) {
    /* TODO(webium): Implement this.
    const tabId = e.detail.tabId;
    const dragOffsetX = e.detail.drag_offset_x;
    const dragOffsetY = e.detail.drag_offset_y;
    this.tabsApi_.detachTab(tabId, dragOffsetX, dragOffsetY);
    */
  }

  // Private methods:
  private registerTabChangeCallbacks_() {
    this.tabsApi_.getTabs().then((tabsSnapshot: TabsSnapshot) => {
      // Bind the observer stream from the snapshot to the callback router
      if (tabsSnapshot.stream && (tabsSnapshot.stream as any).handle) {
        this.tabsApi_.getCallbackRouter().$.bindHandle(
            (tabsSnapshot.stream as any).handle);
      }
    });

    const callbackRouter = this.tabsApi_.getCallbackRouter();
    // TODO(webium): implement these callbacks.
    // callbackRouter.showContextMenu.addListener(
    //     () => this.onShowContextMenu_());
    callbackRouter.onTabsCreated.addListener(this.onTabsCreated_.bind(this));
    // callbackRouter.tabMoved.addListener(this.onTabMoved_.bind(this));
    callbackRouter.onTabsClosed.addListener(this.onTabsClosed_.bind(this));
    callbackRouter.onTabDataChanged.addListener(
        this.onTabDataChanged_.bind(this));
    // callbackRouter.tabReplaced.addListener(this.onTabReplaced_.bind(this));
    // callbackRouter.tabCloseCancelled.addListener(
    //     this.onTabCloseCancelled_.bind(this));
    // callbackRouter.tabGroupStateChanged.addListener(
    //    this.onTabGroupStateChanged_.bind(this));
    // callbackRouter.tabGroupClosed.addListener(
    //     this.onTabGroupClosed_.bind(this));
    // callbackRouter.tabGroupMoved.addListener(
    //     this.onTabGroupMoved_.bind(this));
    callbackRouter.onTabGroupVisualsChanged.addListener(
        this.onTabGroupVisualsChanged_.bind(this));
  }

  private async loadTabStripModel_() {
    const tabSnapshot = await this.tabsApi_.getTabs();
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
    this.layoutManager_.refreshLayout();
    // TODO(webium): Once ContentRegion is implemented:
    // this.contentRegion_.createWebView_(tab.id, tab.active);
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
      // TODO(webium): Once ContentRegion is implemented:
      // this.contentRegion_.removeTab_(tabId);
    });
  }
}
