// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {NodeId} from '/tab_strip_api/tab_strip_api_types.mojom-webui.js';

import type {TabStripItem} from './items.js';
import type {TabElement} from './tab.js';
import type {TabDragHost} from './tab_drag_host.js';

interface DragSession {
  draggedTabId: string;
  initialTabStripItems: TabStripItem[];
  mouseXOffset: number;
  lastMouseX: number;
  containerBounds: DOMRect;
  trailingElementRect: DOMRect|null;
  draggedTabWidth: number;
  draggedTabOriginX: number;
  tabMidpoints: Map<string, number>;
}

export class TabDragDelegate {
  private host_: TabDragHost;
  private session_: DragSession|null = null;

  constructor(host: TabDragHost) {
    this.host_ = host;
  }

  get dragInProgress() {
    return this.session_ !== null;
  }

  onUpdate() {
    if (this.session_) {
      const session = this.session_;
      if (session.draggedTabId &&
          !this.host_.itemsForDrag.some(
              item => item.id === session.draggedTabId)) {
        return;
      }
      for (const element of this.host_.shadowRoot!.querySelectorAll(
               'webui-browser-tab')) {
        if (element.tabData.id !== session.draggedTabId) {
          element.style.transform = '';
        }
      }

      const dragElement = this.host_.getTabElementForDrag(session.draggedTabId);
      if (dragElement) {
        const prevTransform = dragElement.style.transform;
        dragElement.style.transform = '';
        session.draggedTabOriginX = dragElement.getBoundingClientRect().left;
        session.draggedTabWidth = dragElement.offsetWidth;
        dragElement.style.transform = prevTransform;
      }
      this.cacheTabMidpoints_();

      this.moveElementToLocalPoint_(session.lastMouseX);
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
    this.host_.setDragInProgressForDrag(true);
    this.host_.setTabStripNoDrag(true);
    this.host_.activateTabForDrag(nodeId);

    const newTabButton =
        this.host_.shadowRoot?.querySelector<HTMLElement>('#newTabButton');
    const dragElement = this.host_.getTabElementForDrag(nodeId);

    const session: DragSession = {
      draggedTabId: nodeId,
      initialTabStripItems: [...this.host_.itemsForDrag],
      mouseXOffset: tabOriginalOffsetX,
      lastMouseX: localPoint.x,
      containerBounds: this.host_.getDragContainerBounds(),
      trailingElementRect: newTabButton ? newTabButton.getBoundingClientRect() :
                                          null,
      draggedTabWidth: dragElement ? dragElement.offsetWidth : 0,
      draggedTabOriginX:
          dragElement ? dragElement.getBoundingClientRect().left : 0,
      tabMidpoints: new Map(),
    };
    this.session_ = session;
    this.cacheTabMidpoints_();

    // Place the dragged tab at the correct slot based on entry midpoint
    const items = [...session.initialTabStripItems];
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
    if (!this.session_) {
      return;
    }
    const session = this.session_;

    const items = this.host_.itemsForDrag;
    const index = items.findIndex((item: TabStripItem) => {
      return item.type === 'tab' && item.id === session.draggedTabId;
    });
    if (index === -1) {
      return;
    }

    session.lastMouseX = localPoint.x;
    const clampedDeltaX = this.moveElementToLocalPoint_(localPoint.x);

    const dragLeft = session.draggedTabOriginX + clampedDeltaX;
    const dragRight = dragLeft + session.draggedTabWidth;

    if (this.tryMoveLeft_(index, items, dragLeft)) {
      return;
    }
    this.tryMoveRight_(index, items, dragRight);
  }

  onMojoDragLeave() {
    this.clearDragState_();
    this.host_.requestUpdate();
  }

  onMojoDrop(nodeId: NodeId, localPoint: {x: number, y: number}) {
    if (!this.session_) {
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

    this.clearDragState_();
    this.host_.requestUpdate();
  }

  onMojoDragCancelled() {
    if (this.session_ && this.session_.initialTabStripItems.length > 0) {
      const wasOriginallyPresent = this.session_.initialTabStripItems.some(
          item => item.id === this.session_!.draggedTabId);
      if (wasOriginallyPresent) {
        this.host_.setItemsForDrag(this.session_.initialTabStripItems);
      }
    }
    this.clearDragState_();
    this.host_.requestUpdate();
  }

  private cacheTabMidpoints_() {
    if (!this.session_) {
      return;
    }
    this.session_.tabMidpoints.clear();
    for (const item of this.host_.itemsForDrag) {
      if (item.type === 'tab') {
        const element = this.host_.getTabElementForDrag(item.id);
        if (element) {
          const rect = element.getBoundingClientRect();
          const midpoint = rect.left + (rect.width / 2);
          this.session_.tabMidpoints.set(item.id, midpoint);
        }
      }
    }
  }

  private calculateInsertionIndexForPoint_(
      localX: number, items: TabStripItem[]): number {
    if (!this.session_) {
      return items.length;
    }
    const tabItems = items.filter(
        item => item.type === 'tab' && item.id !== this.session_!.draggedTabId);
    for (let i = 0; i < tabItems.length; ++i) {
      const midpoint = this.session_.tabMidpoints.get(tabItems[i]!.id);
      if (midpoint === undefined) {
        continue;
      }
      if (localX < midpoint) {
        return items.findIndex(item => item.id === tabItems[i]!.id);
      }
    }
    return items.length;
  }

  private clearDragState_() {
    if (this.session_) {
      const element =
          this.host_.getTabElementForDrag(this.session_.draggedTabId);
      if (element) {
        element.style.transform = '';
      }
    }
    const newTabButton =
        this.host_.shadowRoot?.querySelector<HTMLElement>('#newTabButton');
    if (newTabButton) {
      newTabButton.style.transform = '';
    }

    this.session_ = null;

    this.host_.setTabStripNoDrag(false);
    this.host_.setDragInProgressForDrag(false);
  }

  private moveElementToLocalPoint_(localX: number): number {
    if (!this.session_) {
      return 0;
    }
    const session = this.session_;
    const tabElement = this.host_.getTabElementForDrag(session.draggedTabId);
    if (!tabElement) {
      return 0;
    }
    const deltaX = localX - session.draggedTabOriginX - session.mouseXOffset;

    // Left boundary clamp:
    const minDeltaX = session.containerBounds.left - session.draggedTabOriginX;

    // Right boundary clamp:
    const buttonWidth = session.trailingElementRect?.width ?? 0;
    const maxButtonOffset = session.trailingElementRect ?
        Math.max(
            0,
            session.containerBounds.right - session.trailingElementRect.right) :
        Infinity;

    const maxDeltaX = session.containerBounds.right - buttonWidth -
        session.draggedTabWidth - session.draggedTabOriginX;
    const clampedDeltaX = Math.min(Math.max(deltaX, minDeltaX), maxDeltaX);
    tabElement.style.transform = `translateX(${clampedDeltaX}px)`;

    const newTabButton =
        this.host_.shadowRoot?.querySelector<HTMLElement>('#newTabButton');
    if (newTabButton && session.trailingElementRect) {
      const draggedRight =
          session.draggedTabOriginX + clampedDeltaX + session.draggedTabWidth;
      if (draggedRight > session.trailingElementRect.left) {
        const offset = Math.min(
            draggedRight - session.trailingElementRect.left, maxButtonOffset);
        newTabButton.style.transform = `translateX(${offset}px)`;
      } else {
        newTabButton.style.transform = '';
      }
    }
    return clampedDeltaX;
  }

  private tryMoveLeft_(index: number, items: TabStripItem[], dragLeft: number):
      boolean {
    if (!this.session_) {
      return false;
    }
    const prevItem = items[index - 1];
    if (prevItem && prevItem.type === 'tab') {
      const targetIdx = index - 1;
      const targetMidpoint = this.session_.tabMidpoints.get(prevItem.id);
      if (targetMidpoint === undefined) {
        return false;
      }
      if (dragLeft < targetMidpoint) {
        [items[index], items[targetIdx]] = [items[targetIdx]!, items[index]!];
        this.host_.setItemsForDrag([...items]);
        return true;
      }
    }
    return false;
  }

  private tryMoveRight_(
      index: number, items: TabStripItem[], dragRight: number): boolean {
    if (!this.session_) {
      return false;
    }
    const nextItem = items[index + 1];
    if (nextItem && nextItem.type === 'tab') {
      const targetIdx = index + 1;
      const targetMidpoint = this.session_.tabMidpoints.get(nextItem.id);
      if (targetMidpoint === undefined) {
        return false;
      }
      if (dragRight > targetMidpoint) {
        [items[index], items[targetIdx]] = [items[targetIdx]!, items[index]!];
        this.host_.setItemsForDrag([...items]);
        return true;
      }
    }
    return false;
  }
}
