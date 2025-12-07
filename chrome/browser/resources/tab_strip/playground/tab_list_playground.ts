// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import '../tab.js';
import '../tab_group.js';

import {TabStripService} from '/tab_strip_api/tab_strip_api.mojom-webui.js';
import type {TabsSnapshot, TabStripServiceRemote} from '/tab_strip_api/tab_strip_api.mojom-webui.js';
import type {Container, Data, SplitTab, Tab, TabCreatedContainer, TabGroup} from '/tab_strip_api/tab_strip_api_data_model.mojom-webui.js';
import type {OnCollectionCreatedEvent, OnDataChangedEvent, OnNodeMovedEvent, OnTabsClosedEvent, OnTabsCreatedEvent} from '/tab_strip_api/tab_strip_api_events.mojom-webui.js';
import type {NodeId, Position} from '/tab_strip_api/tab_strip_api_types.mojom-webui.js';
import {TabStripObservation} from '/tab_strip_api/tab_strip_observation.js';
import type {TabStripObserver} from '/tab_strip_api/tab_strip_observer.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {Color as TabGroupColor} from '../tab_group_types.mojom-webui.js';
import {getTemplate} from '../tab_list.html.js';
import type {TabGroupVisualData} from '../tab_strip.mojom-webui.js';

import {SplitTabElement} from './split_tab_playground.js';
import {TabGroupElement} from './tab_group_playground.js';
import {TabElement} from './tab_playground.js';

export class TabListPlaygroundElement extends CustomElement implements
    TabStripObserver {
  animationPromises: Promise<void>;
  private pinnedTabsElement_: HTMLElement;
  private unpinnedTabsElement_: HTMLElement;
  private tabStripService_: TabStripServiceRemote;
  private tabStripObservation_: TabStripObservation;

  static override get template() {
    return getTemplate();
  }

  constructor() {
    super();
    this.animationPromises = Promise.resolve();
    this.pinnedTabsElement_ = this.getRequiredElement('#pinnedTabs');
    this.unpinnedTabsElement_ = this.getRequiredElement('#unpinnedTabs');
    this.tabStripService_ = TabStripService.getRemote();
    this.tabStripObservation_ = new TabStripObservation(this);
  }

  getIndexOfTab(tabElement: TabElement): number {
    return Array.prototype.indexOf.call(this.$all('tabstrip-tab'), tabElement);
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
  }

  private addAnimationPromise_(promise: Promise<void>) {
    this.animationPromises = this.animationPromises.then(() => promise);
  }

  disconnectedCallback() {}

  private placeElement_(
      element: HTMLElement, index: number, pinned: boolean,
      parentId: string|null|undefined) {
    element.remove();

    if (pinned) {
      this.pinnedTabsElement_.insertBefore(
          element, this.pinnedTabsElement_.childNodes[index]!);
      return;
    }

    let elementToInsert: HTMLElement = element;
    let parentElement = this.unpinnedTabsElement_;

    if (element instanceof TabElement && parentId) {
      let tabGroupElement = this.findNodeElement_(parentId);
      if (tabGroupElement) {
        parentElement = tabGroupElement as TabGroupElement;
      } else {
        // Create the tab group if it doesn't exist.
        tabGroupElement = this.createTabGroupElement_(parentId);
        tabGroupElement.appendChild(element);
        elementToInsert = tabGroupElement;
      }
    }

    let elementAtIndex: HTMLElement|null = null;
    if (parentElement === this.unpinnedTabsElement_) {
      const topLevelChildren =
          Array.from(this.unpinnedTabsElement_.children)
              .filter(
                  e => e instanceof TabElement || e instanceof TabGroupElement);
      elementAtIndex = topLevelChildren[index] || null;
    } else {
      elementAtIndex = parentElement.children[index] as HTMLElement || null;
    }

    if (elementAtIndex) {
      if (elementAtIndex.parentElement instanceof TabGroupElement &&
          elementAtIndex.previousElementSibling === null &&
          elementAtIndex.parentElement !== parentElement) {
        elementAtIndex = elementAtIndex.parentElement;
      }
      elementAtIndex.parentElement!.insertBefore(
          elementToInsert, elementAtIndex);
    } else {
      parentElement.appendChild(elementToInsert);
    }
  }

  onTabsCreated(tabsCreatedEvent: OnTabsCreatedEvent) {
    const tabsCreated: TabCreatedContainer[] = tabsCreatedEvent.tabs;
    tabsCreated.forEach((container) => {
      const tab = container.tab;
      const tabElement = this.createTabElement_(tab, false);
      const position: Position = container.position;
      this.placeElement_(
          tabElement, position.index, false, null /* parent id */);
    });
  }

  onTabsClosed(onTabsClosedEvent: OnTabsClosedEvent) {
    const tabsClosed = onTabsClosedEvent.tabs;
    tabsClosed.forEach((tabId: NodeId) => {
      const element = this.findNodeElement_(tabId);
      if (element instanceof TabElement) {
        this.addAnimationPromise_(element.slideOut());
      }
    });
  }

  onDataChanged(onDataChangedEvent: OnDataChangedEvent) {
    const data = onDataChangedEvent.data;
    if (data.tab) {
      const tab = data.tab;
      const element = this.findNodeElement_(tab.id);
      if (element instanceof TabElement) {
        element.tab = tab;
      }
    } else if (data.tabGroup) {
      const tabGroup = data.tabGroup;
      if (tabGroup) {
        this.findOrCreateTabGroupElement_(tabGroup.id)
            .updateVisuals(this.toTabGroupVisualData_(tabGroup));
      }
    }
  }

  onNodeMoved(event: OnNodeMovedEvent) {
    const element = this.findNodeElement_(event.id);
    if (!element) {
      console.error('Moved element not found:', event.id);
      return;
    }

    let parentId = event.to.parentId;
    if (element instanceof TabGroupElement) {
      parentId = null;
    }
    // For now, assume a tab cannot be moved into the pinned area.
    this.placeElement_(element, event.to.index, false, parentId);
  }

  onCollectionCreated(event: OnCollectionCreatedEvent) {
    const data = event.collection.data;

    if (data.splitTab) {
      this.createSplitTabElement_(data.splitTab);
    } else if (data.tabGroup) {
      // Intentionally not creating a TabGroupElement here. The TabGroupElement
      // will be created when a tab is added to the group in onNodeMoved_, which
      // is fired after this event.
    }
  }
  private onDragEnd_(draggedElement: HTMLElement, x: number, y: number) {
    draggedElement.style.display = 'none';
    const dropTarget = this.shadowRoot!.elementFromPoint(x, y) as HTMLElement;
    draggedElement.style.display = '';

    if (!dropTarget) {
      return;
    }

    let targetParent = dropTarget;
    while (
        targetParent &&
        !targetParent.matches('tabstrip-tab-group-playground, #unpinnedTabs')) {
      targetParent = targetParent.parentElement!;
    }

    if (!targetParent) {
      return;
    }

    let dropTargetElement = dropTarget;
    while (dropTargetElement &&
           dropTargetElement.parentElement !== targetParent) {
      dropTargetElement = dropTargetElement.parentElement!;
    }

    const sourceParent = draggedElement.parentElement!;
    const originalIndex =
        Array.from(sourceParent.children).indexOf(draggedElement);

    let parentId: string|null = null;
    if (draggedElement instanceof TabGroupElement) {
      targetParent = this.unpinnedTabsElement_;
      parentId = null;
    } else if (targetParent.matches('tabstrip-tab-group-playground')) {
      parentId = targetParent.getAttribute('data-node-id');
      targetParent =
          targetParent.shadowRoot!.querySelector<HTMLElement>('#tabs')!;
    }

    let targetIdx =
        Array.from(targetParent.children).indexOf(dropTargetElement);
    if (targetIdx === -1) {
      targetIdx = targetParent.children.length;
    } else {
      const targetRect = dropTargetElement.getBoundingClientRect();
      const isAfterMiddle = x > targetRect.left + targetRect.width / 2;
      if (isAfterMiddle) {
        targetIdx++;
      }
    }

    if (sourceParent === targetParent && originalIndex < targetIdx) {
      targetIdx--;
    }
    this.tabStripService_.moveNode(
        draggedElement.dataset['nodeId']!,
        {parentId: parentId, index: targetIdx});
  }

  private findNodeElement_(nodeId: string): HTMLElement|null {
    if (!nodeId) {
      return null;
    }
    return this.shadowRoot!.querySelector<HTMLElement>(
        `[data-node-id="${nodeId}"]`);
  }

  private clearChildren_(element: HTMLElement) {
    while (element.firstChild) {
      element.removeChild(element.firstChild);
    }
  }

  private fetchAndUpdateTabs_() {
    this.tabStripService_.getTabs().then((tabsSnapshot: TabsSnapshot) => {
      // Bind the observer stream from the snapshot to the callback router
      this.tabStripObservation_.bind(tabsSnapshot.stream.handle);
      console.info('Bound TabsObserver stream to callback router.');

      this.clearChildren_(this.pinnedTabsElement_);
      this.clearChildren_(this.unpinnedTabsElement_);

      if (tabsSnapshot.tabStrip) {
        this.buildTree_(tabsSnapshot.tabStrip, this.shadowRoot!);
      }
    });
  }

  private buildTree_(container: Container, parentDomElement: ParentNode) {
    const data: Data = container.data;
    let currentElement: HTMLElement|null = null;
    let childTargetElement: ParentNode = parentDomElement;

    if (data.tabGroup) {
      const tabGroupElement = this.createTabGroupElement_(data.tabGroup.id);
      tabGroupElement.updateVisuals(this.toTabGroupVisualData_(data.tabGroup));
      currentElement = tabGroupElement;
      childTargetElement = tabGroupElement;
    } else if (data.splitTab) {
      currentElement = this.createSplitTabElement_(data.splitTab);
      childTargetElement = currentElement;
    } else if (data.tab) {
      const isPinned = parentDomElement === this.pinnedTabsElement_;
      currentElement = this.createTabElement_(data.tab, isPinned);
    } else if (data.pinnedTabs) {
      childTargetElement = this.pinnedTabsElement_;
    } else if (data.unpinnedTabs) {
      childTargetElement = this.unpinnedTabsElement_;
    }

    if (currentElement) {
      parentDomElement.appendChild(currentElement);
    }

    container.children.forEach(
        child => this.buildTree_(child, childTargetElement));
  }

  private createTabElement_(tab: Tab, isPinned: boolean): TabElement {
    const tabElement = new TabElement();
    tabElement.tab = tab;
    tabElement.isPinned = isPinned;
    tabElement.setAttribute('data-node-id', tab.id);
    tabElement.dragEndHandler = this.onDragEnd_.bind(this);
    return tabElement;
  }

  private createTabGroupElement_(nodeId: string): TabGroupElement {
    const tabGroupElement = new TabGroupElement();
    tabGroupElement.setAttribute('data-node-id', nodeId);
    tabGroupElement.dragEndHandler = this.onDragEnd_.bind(this);
    this.unpinnedTabsElement_.appendChild(tabGroupElement);
    return tabGroupElement;
  }

  private createSplitTabElement_(splitTab: SplitTab): SplitTabElement {
    console.info('createSplitTabElement');
    const splitTabElement = new SplitTabElement();
    splitTabElement.setAttribute('data-node-id', splitTab.id);
    splitTabElement.dragEndHandler = this.onDragEnd_.bind(this);
    return splitTabElement;
  }

  private findOrCreateTabGroupElement_(groupId: string): TabGroupElement {
    let tabGroupElement = this.findNodeElement_(groupId);
    if (!tabGroupElement) {
      tabGroupElement = this.createTabGroupElement_(groupId);
    }
    return tabGroupElement as TabGroupElement;
  }

  private toTabGroupVisualData_(group: TabGroup): TabGroupVisualData {
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
      title: group.data.title,
      color: colorMap.get(group.data.color)!,
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
