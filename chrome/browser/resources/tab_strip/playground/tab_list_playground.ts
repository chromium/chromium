// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import '../tab.js';
import '../tab_group.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from '../tab_list.html.js';
import type {Container, OnTabDataChangedEvent, OnTabMovedEvent, OnTabsClosedEvent, OnTabsCreatedEvent, OnTabGroupCreatedEvent, OnTabGroupVisualsChangedEvent, Position, Tab, TabCollectionContainer, TabCreatedContainer, TabGroupVisualData as TabsAPI_TabGroupVisualData, NodeId, TabsSnapshot} from '../tab_strip_api.mojom-webui.js';
import type {TabGroupVisualData} from '../tab_strip.mojom-webui.js';
import {Color as TabGroupColor} from '../tab_group_types.mojom-webui.js';
import {TabCollection_CollectionType} from '../tab_strip_api.mojom-webui.js';
import {TabGroupElement} from '../tab_group.js';

import {TabElement} from './tab_playground.js';
import type {TabStripApiProxy} from './tab_strip_api.js';
import {TabStripApiProxyImpl} from './tab_strip_api.js';

export class TabListPlaygroundElement extends CustomElement {
  animationPromises: Promise<void>;
  private tabStripApi_: TabStripApiProxy;
  private pinnedTabsElement_: HTMLElement;
  private unpinnedTabsElement_: HTMLElement;

  static override get template() {
    return getTemplate();
  }

  constructor() {
    super();
    this.animationPromises = Promise.resolve();
    this.tabStripApi_ = TabStripApiProxyImpl.getInstance();
    this.pinnedTabsElement_ = this.getRequiredElement('#pinnedTabs');
    this.unpinnedTabsElement_ = this.getRequiredElement('#unpinnedTabs');
  }

  getIndexOfTab(tabElement: TabElement): number {
    return Array.prototype.indexOf.call(this.$all('tabstrip-tab'), tabElement);
  }

  placeTabElement(
      element: TabElement, index: number, pinned: boolean,
      groupId: string|null|undefined) {
    console.info(
        'Placing TabElement. ID:', element.tab?.id.id, 'at index:', index,
        'Pinned:', pinned, 'GroupId:', groupId);

    // Detach the element from its current parent if it's already in the DOM.
    // This simplifies insertion logic, ensuring it's placed fresh.
    element.remove();

    let targetParent: HTMLElement;
    if (pinned) {
      targetParent = this.pinnedTabsElement_;
    } else if (groupId) {
      targetParent = this.findOrCreateTabGroupElement_(groupId);
    } else {
      // Directly into the unpinnedTabsElement_. If groupId is present, in a
      // full implementation, you would find or create a TabGroupElement and
      // targetParent would become that group element.
      targetParent = this.unpinnedTabsElement_;
    }

    // Insert the element at the specified index within the target parent.
    const childAtIndex = targetParent.childNodes[index];
    if (childAtIndex) {
      targetParent.insertBefore(element, childAtIndex);
    } else {
      targetParent.appendChild(element);
    }
  }

  shouldPreventDrag(isDraggingTab: boolean): boolean {
    if (isDraggingTab) {
      // Do not allow dragging a tab if there's only 1 tab.
      return this.$all('tabstrip-tab').length === 1;
    } else {
      // Do not allow dragging the tab group with no others outside of the tab
      // group. In this case there is only 1 pinned and unpinned top level
      // element, which is the dragging tab group itself.
      return (this.pinnedTabsElement_.childElementCount +
              this.unpinnedTabsElement_.childElementCount) === 1;
    }
  }

  connectedCallback() {
    this.fetchAndUpdateTabs_();
    const callbackRouter = this.tabStripApi_.getCallbackRouter();
    callbackRouter.onTabsCreated.addListener(this.onTabsCreated_.bind(this));
    callbackRouter.onTabsClosed.addListener(this.onTabsClosed_.bind(this));
    callbackRouter.onTabDataChanged.addListener(
        this.onTabDataChanged_.bind(this));
    callbackRouter.onTabMoved.addListener(this.onTabMoved_.bind(this));
    callbackRouter.onTabGroupCreated.addListener(
        this.onTabGroupCreated_.bind(this));
    callbackRouter.onTabGroupVisualsChanged.addListener(
        this.onTabGroupVisualsChanged_.bind(this));
  }

  private addAnimationPromise_(promise: Promise<void>) {
    this.animationPromises = this.animationPromises.then(() => promise);
  }

  disconnectedCallback() {}

  private onTabsCreated_(tabsCreatedEvent: OnTabsCreatedEvent) {
    const tabsCreated: TabCreatedContainer[] = tabsCreatedEvent.tabs;
    tabsCreated.forEach((container) => {
      const tab = container.tab;
      const tabElement = this.createTabElement_(tab, false);
      const position: Position = container.position;
      this.placeTabElement(
          tabElement, position.index, false, null /* parent id */);
    });
  }

  private onTabsClosed_(onTabsClosedEvent: OnTabsClosedEvent) {
    const tabsClosed = onTabsClosedEvent.tabs;
    tabsClosed.forEach((tabId: NodeId) => {
      const tabElement = this.findTabElement_(tabId.id);
      if (tabElement) {
        this.addAnimationPromise_(tabElement.slideOut());
      }
    });
  }

  private onTabDataChanged_(onTabDataChangedEvent: OnTabDataChangedEvent) {
    const tab = onTabDataChangedEvent.tab;
    const tabElement = this.findTabElement_(tab.id.id);
    if (!tabElement) {
      return;
    }
    tabElement.tab = tab;
  }

  private onTabMoved_(event: OnTabMovedEvent) {
    console.info('onTabMoved_', event);
    const element = this.findTabElement_(event.id.id)!;
    element.remove();
    this.placeTabElement(element, event.to.index, false, event.to.parentId?.id);
  }

  private onTabGroupCreated_(event: OnTabGroupCreatedEvent) {
    console.info('onTabGroupCreated_', event);
    // Intentiaonlly not creating a TabGroupElement here. The TabGroupElement
    // will be created when a tab is added to the group in onTabMoved_, which
    // is fired after this event.
  }

  private onTabGroupVisualsChanged_(event: OnTabGroupVisualsChangedEvent) {
    console.info('onTabGroupVisualsChanged_', event);
    this.findOrCreateTabGroupElement_(event.groupId.id).updateVisuals(
      this.toTabGroupVisualData_(event.visualData));
  }

  private createTabElement_(tab: Tab, isPinned: boolean): TabElement {
    const tabElement = new TabElement();
    tabElement.tab = tab;
    tabElement.isPinned = isPinned;
    tabElement.dragEndHandler = (_: TabElement, x: number) => {
      let targetIdx = 0;
      for (const child of this.unpinnedTabsElement_.children) {
        if (x < child.getBoundingClientRect().x) {
          break;
        }
        targetIdx++;
      }
      targetIdx =
          Math.min(targetIdx, this.unpinnedTabsElement_.childElementCount - 1);
      // TODO(crbug.com/412709271): Set the correct parent id.
      this.tabStripApi_.moveTab(tab.id, {parentId: null, index: targetIdx});
    };
    return tabElement;
  }

  private findTabElement_(tabStringId: string): TabElement|null {
    return this.shadowRoot!.querySelector<TabElement>(
        `tabstrip-tab-playground[data-tab-id="${tabStringId}"]`);
  }

  private clearChildren_(element: HTMLElement) {
    while (element.firstChild) {
      element.removeChild(element.firstChild);
    }
  }

  private fetchAndUpdateTabs_() {
    this.tabStripApi_.getTabs().then((tabsSnapshot: TabsSnapshot) => {
      // Bind the observer stream from the snapshot to the callback router
      if (tabsSnapshot.stream && (tabsSnapshot.stream as any).handle) {
        this.tabStripApi_.getCallbackRouter().$.bindHandle(
            (tabsSnapshot.stream as any).handle);
        console.info('Bound TabsObserver stream to callback router.');
      } else {
        console.error('Can not bind');
      }

      this.clearChildren_(this.pinnedTabsElement_);
      this.clearChildren_(this.unpinnedTabsElement_);

      const processContainer =
          (container: TabCollectionContainer, parentIsPinned: boolean) => {
            if (!container || !container.elements) {
              return;
            }
            container.elements.forEach(
                (containerElement: Container, index: number) => {
                  if (containerElement.tabContainer) {
                    const newTab = containerElement.tabContainer.tab;
                    const isPinned = parentIsPinned ||
                        (container.collection &&
                         container.collection.collectionType ===
                             TabCollection_CollectionType.kPinned);

                    let tabElement = this.findTabElement_(newTab.id.id);
                    if (tabElement) {
                      tabElement.tab = newTab;
                      tabElement.isPinned = isPinned;
                    } else {
                      tabElement = this.createTabElement_(newTab, isPinned);
                    }
                    this.placeTabElement(
                        tabElement, index, isPinned,
                        container.collection.id.id ?
                            container.collection.id.id :
                            null);
                  } else if (containerElement.tabCollectionContainer) {
                    const nestedContainer =
                        containerElement.tabCollectionContainer;
                    const collectionIsPinned = parentIsPinned ||
                        (nestedContainer.collection &&
                         nestedContainer.collection.collectionType ===
                             TabCollection_CollectionType.kPinned);
                    processContainer(nestedContainer, collectionIsPinned);
                  }
                });
          };
      if (tabsSnapshot.tabStrip) {
        processContainer(tabsSnapshot.tabStrip, false);
      } else {
        console.info('invalid tab_strip');
      }
    });
  }

  private findTabGroupElement_(groupId: string): TabGroupElement|null {
    return this.$<TabGroupElement>(
        `tabstrip-tab-group[data-group-id="${groupId}"]`);
  }

  private createTabGroupElement_(groupId: string): TabGroupElement {
    const tabGroupElement = new TabGroupElement();
    tabGroupElement.setAttribute('data-group-id', groupId);
    // Adds tab group element under the unpinned tabs element. This follows
    // Monstrudal's implementation.
    this.unpinnedTabsElement_.appendChild(tabGroupElement);
    return tabGroupElement;
  }

  private findOrCreateTabGroupElement_(groupId: string): TabGroupElement {
    let tabGroupElement = this.findTabGroupElement_(groupId);
    if (!tabGroupElement) {
      tabGroupElement = this.createTabGroupElement_(groupId);
    }
    return tabGroupElement;
  }

  private toTabGroupVisualData_(visualData: TabsAPI_TabGroupVisualData): TabGroupVisualData {
    const colorMap = new Map<TabGroupColor, string>([
      [TabGroupColor.kGrey, '128, 128, 128'],
      [TabGroupColor.kBlue, '0, 0, 255'],
      [TabGroupColor.kRed, '255, 0, 0'],
      [TabGroupColor.kYellow, '255, 255, 0'],
      [TabGroupColor.kGreen, '0, 128, 0'],
      [TabGroupColor.kPink, '255, 192, 203'],
      [TabGroupColor.kPurple, '128, 0, 128'],
      [TabGroupColor.kCyan, '0, 255, 255'],
      [TabGroupColor.kOrange, '255, 165, 0'],
   ]);

    return {
      title: visualData.title,
      color: colorMap.get(visualData.color)!,
      textColor: '255, 255, 255' /*white*/,
    };
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tabstrip-playground-tab-list': TabListPlaygroundElement;
  }
}

customElements.define('tabstrip-playground-tab-list', TabListPlaygroundElement);
