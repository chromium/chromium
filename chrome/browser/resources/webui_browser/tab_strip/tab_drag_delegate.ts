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
  private lastMouseEvent_: MouseEvent|null = null;
  // The initial relative position of the left edge of the dragged element to
  // the cursor. This is used to maintain the relative positioing throughout
  // the drag session.
  private mouseXOffset_ = 0;

  // Out of bounds dragOffset.
  private outOfBoundsDragX_ = 0;
  private outOfBoundsDragY_ = 0;

  // Set during drag events.
  private draggedTabId_ = '';
  private dragInProgress_ = false;

  constructor(host: TabDragHost) {
    this.host_ = host;
  }

  get dragInProgress() {
    return this.dragInProgress_;
  }

  onUpdate() {
    if (this.dragInProgress_) {
      assert(this.lastMouseEvent_, 'onUpdate called without lastMouseEvent_');
      for (const element of this.host_.shadowRoot!.querySelectorAll(
               'webui-browser-tab')) {
        // Containers may be reused and rebound to different data during drag.
        // We need to reset the position for all tab elements and retarget the
        // container holding the tab that's being dragged.
        (element as HTMLElement).style.transform = '';
      }
      this.moveElementToCursor_(this.lastMouseEvent_.clientX);
    }
  }

  onMouseDown(e: MouseEvent) {
    e = e || window.event;
    e.preventDefault();
    const path = e.composedPath();
    const tabElement =
        path.find(
            el => el instanceof Element &&
                el.localName === 'webui-browser-tab') as TabElement |
        null;
    if (tabElement) {
      this.draggedTabId_ = tabElement.tabData.id;
      this.dragInProgress_ = true;
      this.host_.setDragInProgressForDrag(true);
      this.outOfBoundsDragX_ = e.clientX;
      this.outOfBoundsDragY_ = e.clientY;
      this.host_.setTabStripNoDrag(true);
      this.host_.activateTabForDrag(this.draggedTabId_);
      this.lastMouseEvent_ = e;
      this.mouseXOffset_ = e.clientX - tabElement.getBoundingClientRect().left;
      this.host_.requestUpdate();
    }
  }

  onMouseUp() {
    if (!this.dragInProgress_) {
      return;
    }

    this.lastMouseEvent_ = null;

    this.getDraggedElement_().style.transform = '';
    this.draggedTabId_ = '';
    this.mouseXOffset_ = 0;
    this.lastMouseEvent_ = null;

    this.host_.setTabStripNoDrag(false);
    this.dragInProgress_ = false;
    this.host_.setDragInProgressForDrag(false);
    this.host_.requestUpdate();
  }

  onMouseMove(e: MouseEvent) {
    e = e || window.event;
    e.preventDefault();
    if (!this.dragInProgress_) {
      return;
    }
    this.lastMouseEvent_ = e;

    // Move the tab to its new position relative to the current cursor position.
    this.moveElementToCursor_(e.clientX);
    const dragElementRect = this.getDraggedElement_().getBoundingClientRect();

    // Now we will test the positioning after the DOM has been laid out.
    const items = this.host_.itemsForDrag;
    const index = items.findIndex((item: TabStripItem) => {
      return item.type === 'tab' && item.id === this.draggedTabId_;
    });
    assert(index !== -1, 'dragged tab not found in items_');
    // Test for swap forward case.
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
      }
    }
    // Test for swap backward case.
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
      }
    }

    // Check if tab is being dragged outside of bounds +/- artificial margins.
    if (e.clientX < this.host_.getBoundingClientRect().left ||
        e.clientX >= this.host_.getBoundingClientRect().right - 1 ||
        e.clientY < this.host_.getBoundingClientRect().top ||
        e.clientY > this.host_.getBoundingClientRect().bottom + 10) {
      this.outOfBoundsHandler_(this.draggedTabId_);
    }
  }

  private getDraggedElement_(): TabElement {
    assert(this.dragInProgress_ && this.draggedTabId_, 'drag not in progress');
    const element = this.host_.getTabElementForDrag(this.draggedTabId_);
    assert(element, 'dragged tab element not found');
    return element;
  }

  // Moves the dragged element to its relative position to the cursor at the
  // start of the drag.
  private moveElementToCursor_(mouseClientX: number) {
    assert(this.dragInProgress_, 'moveElementToCursor_ called without drag');

    const tabElement = this.getDraggedElement_();
    // Using the mouse cursor as a frame of reference, compute where the tab
    // element needs to be to maintain the same relative position at the
    // start of the drag.
    // First we reset the transformation so we know where the element should
    // be.
    tabElement.style.transform = '';
    const deltaX = mouseClientX - tabElement.getBoundingClientRect().left -
        this.mouseXOffset_;
    tabElement.style.transform = `translateX(${deltaX}px)`;
  }

  private outOfBoundsHandler_(tabId: NodeId) {
    this.host_.fire('tab-drag-out-of-bounds', {
      tabId: tabId,
      drag_offset_x: this.outOfBoundsDragX_,
      drag_offset_y: this.outOfBoundsDragY_,
    });
    this.onMouseUp();
  }
}
