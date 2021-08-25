// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';

import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {getBookmarkFromDragEvent, isBookmarkFolderElement, isValidDropTarget} from './bookmark_folder.js';

export const DROP_POSITION_ATTR = 'drop-position';

export enum DropPosition {
  ABOVE = 'above',
  INTO = 'into',
  BELOW = 'below',
}

interface BookmarksDragDelegate extends HTMLElement {}

class DragSession {
  private dragData_: chrome.bookmarkManagerPrivate.DragData;
  private lastDragOverElement_: HTMLElement|null = null;
  private lastPointerWasTouch_ = false;

  constructor(dragData: chrome.bookmarkManagerPrivate.DragData) {
    this.dragData_ = dragData;
  }

  start(e: DragEvent) {
    chrome.bookmarkManagerPrivate.startDrag(
        this.dragData_.elements!.map(bookmark => bookmark.id), 0,
        this.lastPointerWasTouch_, e.clientX, e.clientY);
  }

  update(e: DragEvent) {
    const dragOverElement = e.composedPath().find(target => {
      return target instanceof HTMLElement && isValidDropTarget(target);
    }) as HTMLElement;
    if (!dragOverElement) {
      return;
    }

    if (dragOverElement !== this.lastDragOverElement_) {
      this.clearStyles_();
    }

    const isDraggingOverFolder = isBookmarkFolderElement(dragOverElement);
    const dragOverElRect = dragOverElement.getBoundingClientRect();
    const dragOverYRatio =
        (e.clientY - dragOverElRect.top) / dragOverElRect.height;

    let dropPosition;
    if (isDraggingOverFolder) {
      if (dragOverYRatio <= .25) {
        dropPosition = DropPosition.ABOVE;
      } else if (dragOverYRatio <= .75) {
        dropPosition = DropPosition.INTO;
      } else {
        dropPosition = DropPosition.BELOW;
      }
    } else {
      dropPosition =
          dragOverYRatio <= .5 ? DropPosition.ABOVE : DropPosition.BELOW;
    }
    dragOverElement.setAttribute(DROP_POSITION_ATTR, dropPosition);
    this.lastDragOverElement_ = dragOverElement;
  }

  cancel() {
    this.clearStyles_();
  }

  private clearStyles_() {
    if (!this.lastDragOverElement_) {
      return;
    }
    this.lastDragOverElement_.removeAttribute(DROP_POSITION_ATTR);
  }

  static createFromBookmark(bookmark: chrome.bookmarks.BookmarkTreeNode) {
    return new DragSession({
      elements: [bookmark],
      sameProfile: true,
    });
  }
}

export class BookmarksDragManager {
  private delegate_: BookmarksDragDelegate;
  private dragSession_: DragSession|null;
  private eventTracker_: EventTracker = new EventTracker();

  constructor(delegate: BookmarksDragDelegate) {
    this.delegate_ = delegate;
  }

  startObserving() {
    this.eventTracker_.add(
        this.delegate_, 'dragstart', e => this.onDragStart_(e as DragEvent));
    this.eventTracker_.add(
        this.delegate_, 'dragover', e => this.onDragOver_(e as DragEvent));
    this.eventTracker_.add(
        this.delegate_, 'dragleave', () => this.onDragLeave_());
    this.eventTracker_.add(this.delegate_, 'dragend', () => this.cancelDrag_());
  }

  stopObserving() {
    this.eventTracker_.removeAll();
  }

  private cancelDrag_() {
    if (!this.dragSession_) {
      return;
    }
    this.dragSession_.cancel();
    this.dragSession_ = null;
  }

  private onDragStart_(e: DragEvent) {
    e.preventDefault();
    if (!loadTimeData.getBoolean('bookmarksDragAndDropEnabled')) {
      return;
    }

    const bookmark = getBookmarkFromDragEvent(e);
    if (!bookmark ||
        /* Cannot drag root's children. */ bookmark.parentId === '0' ||
        bookmark.unmodifiable) {
      return;
    }

    this.dragSession_ = DragSession.createFromBookmark(bookmark);
    this.dragSession_.start(e);
  }

  private onDragOver_(e: DragEvent) {
    e.preventDefault();
    if (!this.dragSession_) {
      return;
    }
    this.dragSession_.update(e);
  }

  private onDragLeave_() {
    if (!this.dragSession_) {
      return;
    }

    this.dragSession_.cancel();
  }
}