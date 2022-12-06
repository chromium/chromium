// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';

import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {getBookmarkFromElement, isBookmarkFolderElement, isValidDropTarget} from './bookmark_folder.js';

export const DROP_POSITION_ATTR = 'drop-position';

const ROOT_FOLDER_ID = '0';

// Ms to wait during a dragover to open closed folder.
let folderOpenerTimeoutDelay = 1000;
export function overrideFolderOpenerTimeoutDelay(ms: number) {
  folderOpenerTimeoutDelay = ms;
}

export enum DropPosition {
  ABOVE = 'above',
  INTO = 'into',
  BELOW = 'below',
}

interface BookmarksDragDelegate extends HTMLElement {
  getAscendants(bookmarkId: string): string[];
  getIndex(bookmark: chrome.bookmarks.BookmarkTreeNode): number;
  isFolderOpen(bookmark: chrome.bookmarks.BookmarkTreeNode): boolean;
  onFinishDrop(bookmarks: chrome.bookmarks.BookmarkTreeNode[]): void;
  openFolder(folderId: string): void;
}

class DragSession {
  private delegate_: BookmarksDragDelegate;
  private dragData_: chrome.bookmarkManagerPrivate.DragData;
  private lastDragOverElement_: HTMLElement|null = null;
  private lastPointerWasTouch_ = false;
  private folderOpenerTimeout_: number|null = null;

  constructor(
      delegate: BookmarksDragDelegate,
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
      return target instanceof HTMLElement && isValidDropTarget(target);
    }) as HTMLElement;
    if (!dragOverElement) {
      return;
    }

    if (dragOverElement !== this.lastDragOverElement_) {
      this.resetState_();
    }

    const dragOverBookmark = getBookmarkFromElement(dragOverElement);
    const ascendants = this.delegate_.getAscendants(dragOverBookmark.id);
    const isInvalidDragOverTarget = dragOverBookmark.unmodifiable ||
        this.dragData_.elements &&
            this.dragData_.elements.some(
                element => ascendants.indexOf(element.id) !== -1);
    if (isInvalidDragOverTarget) {
      this.lastDragOverElement_ = null;
      return;
    }

    const isDraggingOverFolder = isBookmarkFolderElement(dragOverElement);
    const dragOverElRect = dragOverElement.getBoundingClientRect();
    const dragOverYRatio =
        (e.clientY - dragOverElRect.top) / dragOverElRect.height;

    let dropPosition;
    if (isDraggingOverFolder) {
      const folderIsOpen = this.delegate_.isFolderOpen(dragOverBookmark);
      if (dragOverBookmark.parentId === ROOT_FOLDER_ID) {
        // Cannot drag above or below children of root folder.
        dropPosition = DropPosition.INTO;
      } else if (dragOverYRatio <= .25) {
        dropPosition = DropPosition.ABOVE;
      } else if (dragOverYRatio <= .75) {
        dropPosition = DropPosition.INTO;
      } else if (folderIsOpen) {
        // If a folder is open, its child bookmarks appear immediately below it
        // so it should not be possible to drop a bookmark right below an open
        // folder.
        dropPosition = DropPosition.INTO;
      } else {
        dropPosition = DropPosition.BELOW;
      }
    } else {
      dropPosition =
          dragOverYRatio <= .5 ? DropPosition.ABOVE : DropPosition.BELOW;
    }
    dragOverElement.setAttribute(DROP_POSITION_ATTR, dropPosition);

    if (dropPosition === DropPosition.INTO &&
        !this.delegate_.isFolderOpen(dragOverBookmark) &&
        !this.folderOpenerTimeout_) {
      // Queue a timeout to auto-open the dragged over folder.
      this.folderOpenerTimeout_ = setTimeout(() => {
        this.delegate_.openFolder(dragOverBookmark.id);
        this.folderOpenerTimeout_ = null;
      }, folderOpenerTimeoutDelay);
    }

    this.lastDragOverElement_ = dragOverElement;
  }

  cancel() {
    this.resetState_();
    this.lastDragOverElement_ = null;
  }

  finish() {
    if (!this.lastDragOverElement_) {
      return;
    }

    const dropTargetBookmark =
        getBookmarkFromElement(this.lastDragOverElement_);
    const dropPosition = this.lastDragOverElement_.getAttribute(
                             DROP_POSITION_ATTR) as DropPosition;
    this.resetState_();

    if (isBookmarkFolderElement(this.lastDragOverElement_) &&
        dropPosition === DropPosition.INTO) {
      chrome.bookmarkManagerPrivate.drop(
          dropTargetBookmark.id, /* index */ undefined,
          () => this.delegate_.onFinishDrop(this.dragData_.elements!));
      return;
    }

    let toIndex = this.delegate_.getIndex(dropTargetBookmark);
    toIndex += dropPosition === DropPosition.BELOW ? 1 : 0;
    chrome.bookmarkManagerPrivate.drop(
        dropTargetBookmark.parentId!, toIndex,
        () => this.delegate_.onFinishDrop(this.dragData_.elements!));
  }

  private resetState_() {
    if (this.lastDragOverElement_) {
      this.lastDragOverElement_.removeAttribute(DROP_POSITION_ATTR);
    }

    if (this.folderOpenerTimeout_ !== null) {
      clearTimeout(this.folderOpenerTimeout_);
      this.folderOpenerTimeout_ = null;
    }
  }

  static createFromBookmark(
      delegate: BookmarksDragDelegate,
      bookmark: chrome.bookmarks.BookmarkTreeNode) {
    return new DragSession(delegate, {
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

    if (loadTimeData.getBoolean('bookmarksDragAndDropEnabled')) {
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
    if (!loadTimeData.getBoolean('bookmarksDragAndDropEnabled')) {
      return;
    }

    const bookmark = getBookmarkFromElement(
        e.composedPath().find(target => (target as HTMLElement).draggable) as
        HTMLElement);
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
