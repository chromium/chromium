// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import '../tab.js';
import '../tab_group.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from '../tab_list.html.js';
import type {Container, OnTabDataChangedEvent, OnTabsClosedEvent, OnTabsCreatedEvent, Position, Tab, TabCollectionContainer, TabCreatedContainer, TabId, TabsSnapshot} from '../tab_strip_api.mojom-webui.js';
import {TabCollection_CollectionType} from '../tab_strip_api.mojom-webui.js';

import {TabElement} from './tab_playground.js';
import type {TabStripApiProxy} from './tab_strip_api.js';
import {TabStripApiProxyImpl} from './tab_strip_api.js';

export class TabListPlaygroundElement extends CustomElement {
  animationPromises: Promise<void>;
  private pinnedTabsElement_: HTMLElement;
  private tabStripApi_: TabStripApiProxy;
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
      groupId: string|null) {
    console.info(
        'Placing TabElement. ID:', element.tab?.id.id, 'at index:', index,
        'Pinned:', pinned, 'GroupId:', groupId);

    // Detach the element from its current parent if it's already in the DOM.
    // This simplifies insertion logic, ensuring it's placed fresh.
    element.remove();

    let targetParent: HTMLElement;
    if (pinned) {
      targetParent = this.pinnedTabsElement_;
    } else {
      // TODO: Implement tab group handling. For now, all unpinned tabs go
      // directly into the unpinnedTabsElement_. If groupId is present, in a
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
    tabsClosed.forEach((tabId: TabId) => {
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

  private createTabElement_(tab: Tab, isPinned: boolean): TabElement {
    const tabElement = new TabElement();
    tabElement.tab = tab;
    tabElement.isPinned = isPinned;
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
}

declare global {
  interface HTMLElementTagNameMap {
    'tabstrip-playground-tab-list': TabListPlaygroundElement;
  }
}

customElements.define('tabstrip-playground-tab-list', TabListPlaygroundElement);
