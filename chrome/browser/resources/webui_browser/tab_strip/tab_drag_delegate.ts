// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
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
      const tabOriginalOffsetX = Math.round(tabRect.left);
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

    // Calculate mouse offset inside the tab at drag start using
    // tabOriginalOffsetX
    this.mouseXOffset_ = localPoint.x - tabOriginalOffsetX;

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

    const dragElementRect = this.getDraggedElement_().getBoundingClientRect();
    if (this.tryMoveLeft_(index, items, dragElementRect)) {
      return;
    }
    this.tryMoveRight_(index, items, dragElementRect);
  }

  onMojoDragLeave() {
    this.clearDragState_();
    this.host_.requestUpdate();
  }

  onMojoDrop(nodeId: NodeId, _localPoint: {x: number, y: number}) {
    if (!this.dragInProgress_) {
      return;
    }

    const items = this.host_.itemsForDrag;
    const index = items.findIndex((item: TabStripItem) => {
      return item.type === 'tab' && item.id === nodeId;
    });
    assert(index !== -1, 'dropped tab not found in items_');

    // Commit the drag to the host (calls TabStripService.moveNode)
    this.host_.commitDrag(nodeId, index);

    this.originalItems_ = null;  // Successful drop, don't revert
    this.clearDragState_();
    this.host_.requestUpdate();
  }

  onMojoDragCancelled() {
    if (this.originalItems_) {
      this.host_.setItemsForDrag(this.originalItems_);
    }
    this.clearDragState_();
    this.host_.requestUpdate();
  }

  private clearDragState_() {
    if (this.dragInProgress_ && this.draggedTabId_) {
      const element = this.host_.getTabElementForDrag(this.draggedTabId_);
      if (element) {
        element.style.transform = '';
      }
    }
    this.draggedTabId_ = '';
    this.mouseXOffset_ = 0;
    this.dragInProgress_ = false;
    this.originalItems_ = null;

    this.host_.setTabStripNoDrag(false);
    this.host_.setDragInProgressForDrag(false);
  }

  private moveElementToLocalPoint_(localX: number) {
    const tabElement = this.getDraggedElement_();
    tabElement.style.transform = '';
    const originalViewportLeft = tabElement.getBoundingClientRect().left;
    const deltaX = localX - originalViewportLeft - this.mouseXOffset_;
    tabElement.style.transform = `translateX(${deltaX}px)`;
  }

  private tryMoveLeft_(
      index: number, items: TabStripItem[], dragElementRect: DOMRect): boolean {
    const prevItem = items[index - 1];
    if (prevItem && prevItem.type === 'tab') {
      const targetIdx = index - 1;
      const target = this.host_.getTabElementForDrag(prevItem.id);
      assert(target, 'prev tab element not found');
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
      assert(target, 'next tab element not found');
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

  private getDraggedElement_(): TabElement {
    assert(this.dragInProgress_ && this.draggedTabId_, 'drag not in progress');
    const element = this.host_.getTabElementForDrag(this.draggedTabId_);
    assert(element, 'dragged tab element not found');
    return element;
  }
}
