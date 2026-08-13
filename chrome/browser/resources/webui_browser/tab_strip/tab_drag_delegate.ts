// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {NodeId} from '/tab_strip_api/tab_strip_api_types.mojom-webui.js';

import type {TabStripItem} from './items.js';
import type {TabElement} from './tab.js';
import type {TabDragHost} from './tab_drag_host.js';

export class TabDragDelegate {
  private host_: TabDragHost;

  // Drag experience variables.
  private mouseXOffset_ = 0;
  private draggedTabId_ = '';
  private dragInProgress_ = false;
  private originalItems_: TabStripItem[]|null = null;
  private lastLocalX_ = 0;

  constructor(host: TabDragHost) {
    this.host_ = host;
  }

  get dragInProgress() {
    return this.dragInProgress_;
  }

  onUpdate() {
    if (this.dragInProgress_) {
      if (this.draggedTabId_ &&
          !this.host_.itemsForDrag.some(
              item => item.id === this.draggedTabId_)) {
        return;
      }
      for (const element of this.host_.shadowRoot!.querySelectorAll(
               'webui-browser-tab')) {
        if (element.tabData.id !== this.draggedTabId_) {
          element.style.transform = '';
        }
      }
      this.moveElementToLocalPoint_(this.lastLocalX_);
    }
  }

  onMouseDown(e: MouseEvent) {
    // Prevent starting a drag if the user clicked the close button.
    const path = e.composedPath();
    const isCloseButton = path.some(
        el => el instanceof Element && el.classList.contains('close'));
    if (isCloseButton) {
      return;
    }

    const tabElement = this.findTabElement_(e);
    if (tabElement) {
      e.preventDefault();
      const nodeId = tabElement.tabData.id;
      const startPoint = {x: Math.round(e.screenX), y: Math.round(e.screenY)};
      const tabRect = tabElement.getBoundingClientRect();
      const tabOriginalOffsetX = Math.round(e.clientX - tabRect.left);
      this.host_.tabDragService.startDrag(
          [nodeId], startPoint, tabOriginalOffsetX);
    }
  }

  private findTabElement_(e: MouseEvent): TabElement|null {
    const path = e.composedPath();
    return path.find(
               el => el instanceof Element &&
                   el.localName === 'webui-browser-tab') as TabElement |
        null;
  }

  // Mojo Drag Callbacks
  onMojoDragEntered(
      nodeId: NodeId, localPoint: {x: number, y: number},
      tabOriginalOffsetX: number) {
    this.draggedTabId_ = nodeId;
    this.dragInProgress_ = true;
    this.lastLocalX_ = localPoint.x;
    this.host_.setDragInProgressForDrag(true);
    this.host_.setTabStripNoDrag(true);
    this.host_.activateTabForDrag(this.draggedTabId_);

    // Save original items for cancel/revert
    this.originalItems_ = [...this.host_.itemsForDrag];

    // Store invariant mouse offset inside the tab
    this.mouseXOffset_ = tabOriginalOffsetX;

    // Place the dragged tab at the correct slot based on entry midpoint
    const items = [...this.host_.itemsForDrag];
    const targetIndex =
        this.calculateInsertionIndexForPoint_(localPoint.x, items);
    const draggedItemIndex = items.findIndex(item => item.id === nodeId);
    if (draggedItemIndex !== -1 && draggedItemIndex !== targetIndex) {
      const [draggedItem] = items.splice(draggedItemIndex, 1);
      const insertAt =
          targetIndex > draggedItemIndex ? targetIndex - 1 : targetIndex;
      items.splice(insertAt, 0, draggedItem!);
      this.host_.setItemsForDrag(items);
    }

    this.host_.requestUpdate();
  }

  onMojoDrag(localPoint: {x: number, y: number}) {
    if (!this.dragInProgress_) {
      return;
    }

    const items = this.host_.itemsForDrag;
    const index = items.findIndex((item: TabStripItem) => {
      return item.type === 'tab' && item.id === this.draggedTabId_;
    });
    if (index === -1) {
      return;
    }

    this.lastLocalX_ = localPoint.x;
    this.moveElementToLocalPoint_(localPoint.x);

    const dragElement = this.host_.getTabElementForDrag(this.draggedTabId_);
    if (!dragElement) {
      return;
    }
    const dragElementRect = dragElement.getBoundingClientRect();
    if (this.tryMoveLeft_(index, items, dragElementRect)) {
      return;
    }
    this.tryMoveRight_(index, items, dragElementRect);
  }

  onMojoDragLeave() {
    this.clearDragState_();
    this.host_.requestUpdate();
  }

  onMojoDrop(nodeId: NodeId, localPoint: {x: number, y: number}) {
    if (!this.dragInProgress_) {
      return;
    }

    const items = this.host_.itemsForDrag;
    let index = items.findIndex((item: TabStripItem) => {
      return item.type === 'tab' && item.id === nodeId;
    });
    if (index === -1) {
      index = this.calculateInsertionIndexForPoint_(localPoint.x, items);
    }

    // Commit the drag to the host (calls TabStripService.moveNode)
    this.host_.commitDrag(nodeId, index);

    this.originalItems_ = null;  // Successful drop, don't revert
    this.clearDragState_();
    this.host_.requestUpdate();
  }

  onMojoDragCancelled() {
    if (this.originalItems_) {
      const wasOriginallyPresent =
          this.originalItems_.some(item => item.id === this.draggedTabId_);
      if (wasOriginallyPresent) {
        this.host_.setItemsForDrag(this.originalItems_);
      }
    }
    this.clearDragState_();
    this.host_.requestUpdate();
  }

  private calculateInsertionIndexForPoint_(
      localX: number, items: TabStripItem[]): number {
    const tabItems = items.filter(
        item => item.type === 'tab' && item.id !== this.draggedTabId_);
    for (let i = 0; i < tabItems.length; ++i) {
      const tabElement = this.host_.getTabElementForDrag(tabItems[i]!.id);
      if (!tabElement) {
        continue;
      }
      const rect = tabElement.getBoundingClientRect();
      const midpoint = rect.left + (rect.width / 2);
      if (localX < midpoint) {
        return items.findIndex(item => item.id === tabItems[i]!.id);
      }
    }
    return items.length;
  }

  private clearDragState_() {
    if (this.dragInProgress_ && this.draggedTabId_) {
      const element = this.host_.getTabElementForDrag(this.draggedTabId_);
      if (element) {
        element.style.transform = '';
      }
    }
    const newTabButton =
        this.host_.shadowRoot?.querySelector<HTMLElement>('#newTabButton');
    if (newTabButton) {
      newTabButton.style.transform = '';
    }
    this.draggedTabId_ = '';
    this.mouseXOffset_ = 0;
    this.dragInProgress_ = false;
    this.originalItems_ = null;

    this.host_.setTabStripNoDrag(false);
    this.host_.setDragInProgressForDrag(false);
  }

  private moveElementToLocalPoint_(localX: number) {
    const tabElement = this.host_.getTabElementForDrag(this.draggedTabId_);
    if (!tabElement) {
      return;
    }
    tabElement.style.transform = '';
    const originalViewportLeft = tabElement.getBoundingClientRect().left;
    const deltaX = localX - originalViewportLeft - this.mouseXOffset_;
    const containerBounds = this.host_.getDragContainerBounds();

    // Left boundary clamp:
    const minDeltaX = containerBounds.left - originalViewportLeft;

    // Right boundary clamp (Add Tab button geometry):
    const newTabButton =
        this.host_.shadowRoot?.querySelector<HTMLElement>('#newTabButton');
    if (newTabButton) {
      newTabButton.style.transform = '';
    }
    const buttonRect = newTabButton?.getBoundingClientRect();
    const buttonWidth = buttonRect?.width ?? 0;
    const maxButtonOffset = buttonRect ?
        Math.max(0, containerBounds.right - buttonRect.right) :
        Infinity;

    const maxDeltaX = containerBounds.right - buttonWidth -
        tabElement.offsetWidth - originalViewportLeft;
    const clampedDeltaX = Math.min(Math.max(deltaX, minDeltaX), maxDeltaX);
    tabElement.style.transform = `translateX(${clampedDeltaX}px)`;

    if (newTabButton && buttonRect) {
      const draggedRight =
          originalViewportLeft + clampedDeltaX + tabElement.offsetWidth;
      if (draggedRight > buttonRect.left) {
        const offset =
            Math.min(draggedRight - buttonRect.left, maxButtonOffset);
        newTabButton.style.transform = `translateX(${offset}px)`;
      }
    }
  }

  private tryMoveLeft_(
      index: number, items: TabStripItem[], dragElementRect: DOMRect): boolean {
    const prevItem = items[index - 1];
    if (prevItem && prevItem.type === 'tab') {
      const targetIdx = index - 1;
      const target = this.host_.getTabElementForDrag(prevItem.id);
      if (!target) {
        return false;
      }
      const targetMidpoint = target.getBoundingClientRect().left +
          (target.getBoundingClientRect().width / 2);
      if (dragElementRect.left < targetMidpoint) {
        [items[index], items[targetIdx]] = [items[targetIdx]!, items[index]!];
        this.host_.setItemsForDrag([...items]);
        return true;
      }
    }
    return false;
  }

  private tryMoveRight_(
      index: number, items: TabStripItem[], dragElementRect: DOMRect): boolean {
    const nextItem = items[index + 1];
    if (nextItem && nextItem.type === 'tab') {
      const targetIdx = index + 1;
      const target = this.host_.getTabElementForDrag(nextItem.id);
      if (!target) {
        return false;
      }
      const targetMidpoint = target.getBoundingClientRect().left +
          (target.getBoundingClientRect().width / 2);
      if (dragElementRect.right > targetMidpoint) {
        [items[index], items[targetIdx]] = [items[targetIdx]!, items[index]!];
        this.host_.setItemsForDrag([...items]);
        return true;
      }
    }
    return false;
  }
}
