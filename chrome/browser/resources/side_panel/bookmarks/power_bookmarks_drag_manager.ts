// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';

import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {PowerBookmarkRowElement} from './power_bookmark_row.js';

const ROOT_FOLDER_ID = '0';

export const DROP_POSITION_ATTR = 'drop-position';

export enum DropPosition {
  INTO = 'into',
}

interface PowerBookmarksDragDelegate extends HTMLElement {
  getFallbackBookmark(): chrome.bookmarks.BookmarkTreeNode;
  getFallbackDropTargetElement(): HTMLElement;
  onFinishDrop(dropTarget: chrome.bookmarks.BookmarkTreeNode): void;
}

class DragSession {
  private delegate_: PowerBookmarksDragDelegate;
  private dragData_: chrome.bookmarkManagerPrivate.DragData;
  private lastDragOverElement_: PowerBookmarkRowElement|null = null;
  private lastDropTargetBookmark_: chrome.bookmarks.BookmarkTreeNode|null =
      null;
  private lastPointerWasTouch_ = false;

  constructor(
      delegate: PowerBookmarksDragDelegate,
      dragData: chrome.bookmarkManagerPrivate.DragData) {
    this.delegate_ = delegate;
    this.dragData_ = dragData;
  }

  start(e: DragEvent) {
    chrome.bookmarkManagerPrivate.startDrag(
        this.dragData_.elements!.map(bookmark => bookmark.id), 0,
        this.lastPointerWasTouch_, e.clientX, e.clientY);
  }

  update(e: DragEvent) {
    const dragOverElement = e.composedPath().find(target => {
      return target instanceof PowerBookmarkRowElement;
    }) as PowerBookmarkRowElement;

    if (!dragOverElement) {
      // Invalid drag over element. Cancel session.
      this.cancel();
      return;
    } else if (dragOverElement === this.lastDragOverElement_) {
      // State has not changed, nothing to update.
      return;
    }

    this.resetState_();

    const dragOverBookmark = dragOverElement.bookmark;
    let dropTargetBookmark = dragOverBookmark;

    const invalidDropTarget = dropTargetBookmark.unmodifiable ||
        dropTargetBookmark.url ||
        (this.dragData_.elements &&
         this.dragData_.elements.some(
             element => element.id === dropTargetBookmark.id));
    if (invalidDropTarget) {
      dropTargetBookmark = this.delegate_.getFallbackBookmark();
    }

    const draggedBookmarks = this.dragData_.elements!;
    let dropTargetIsParent = true;
    draggedBookmarks.forEach((bookmark: chrome.bookmarks.BookmarkTreeNode) => {
      if (bookmark.parentId !== dropTargetBookmark.id) {
        dropTargetIsParent = false;
      }
    });
    if (draggedBookmarks.length === 0 || dropTargetIsParent) {
      this.cancel();
      return;
    }

    if (dragOverBookmark.url) {
      this.delegate_.getFallbackDropTargetElement().setAttribute(
          DROP_POSITION_ATTR, DropPosition.INTO);
    } else {
      dragOverElement.setAttribute(DROP_POSITION_ATTR, DropPosition.INTO);
    }
    this.lastDragOverElement_ = dragOverElement;
    this.lastDropTargetBookmark_ = dropTargetBookmark;
  }

  cancel() {
    this.resetState_();
    this.lastDragOverElement_ = null;
    this.lastDropTargetBookmark_ = null;
  }

  finish() {
    // TODO(crbug.com/40267573): Ensure it is possible to drag bookmarks into an
    // empty active folder.
    if (!this.lastDropTargetBookmark_) {
      return;
    }
    chrome.bookmarkManagerPrivate
        .drop(this.lastDropTargetBookmark_.id, /* index */ undefined)
        .then(() => {
          this.delegate_.onFinishDrop(this.lastDropTargetBookmark_!);
          this.cancel();
        });
  }

  private resetState_() {
    if (this.lastDragOverElement_) {
      this.lastDragOverElement_.removeAttribute(DROP_POSITION_ATTR);
    }
    this.delegate_.getFallbackDropTargetElement().removeAttribute(
        DROP_POSITION_ATTR);
  }

  static createFromBookmark(
      delegate: PowerBookmarksDragDelegate,
      bookmark: chrome.bookmarks.BookmarkTreeNode) {
    return new DragSession(delegate, {
      elements: [bookmark],
      sameProfile: true,
    });
  }
}

export class PowerBookmarksDragManager {
  private delegate_: PowerBookmarksDragDelegate;
  private dragSession_: DragSession|null;
  private eventTracker_: EventTracker = new EventTracker();

  constructor(delegate: PowerBookmarksDragDelegate) {
    this.delegate_ = delegate;
  }

  startObserving() {
    this.eventTracker_.removeAll();
    this.eventTracker_.add(
        this.delegate_, 'dragstart',
        (e: Event) => this.onDragStart_(e as DragEvent));
    this.eventTracker_.add(
        this.delegate_, 'dragover',
        (e: Event) => this.onDragOver_(e as DragEvent));
    this.eventTracker_.add(
        this.delegate_, 'dragleave', () => this.onDragLeave_());
    this.eventTracker_.add(this.delegate_, 'dragend', () => this.cancelDrag_());
    this.eventTracker_.add(
        this.delegate_, 'drop', (e: Event) => this.onDrop_(e as DragEvent));

    if (loadTimeData.getBoolean('editBookmarksEnabled')) {
      chrome.bookmarkManagerPrivate.onDragEnter.addListener(
          (dragData: chrome.bookmarkManagerPrivate.DragData) =>
              this.onChromeDragEnter_(dragData));
      chrome.bookmarkManagerPrivate.onDragLeave.addListener(
          () => this.cancelDrag_());
    }
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

  private onChromeDragEnter_(dragData: chrome.bookmarkManagerPrivate.DragData) {
    if (this.dragSession_) {
      // A drag session is already in flight.
      return;
    }

    this.dragSession_ = new DragSession(this.delegate_, dragData);
  }

  private onDragStart_(e: DragEvent) {
    e.preventDefault();
    if (!loadTimeData.getBoolean('editBookmarksEnabled')) {
      return;
    }

    const bookmark =
        (e.composedPath().find(target => (target as HTMLElement).draggable) as
         PowerBookmarkRowElement)
            .bookmark;
    if (!bookmark ||
        /* Cannot drag root's children. */ bookmark.parentId ===
            ROOT_FOLDER_ID ||
        bookmark.unmodifiable) {
      return;
    }

    this.dragSession_ =
        DragSession.createFromBookmark(this.delegate_, bookmark);
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

  private onDrop_(e: DragEvent) {
    if (!this.dragSession_) {
      return;
    }

    e.preventDefault();
    this.dragSession_.finish();
    this.dragSession_ = null;
  }
}
