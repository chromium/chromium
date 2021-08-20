// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';

import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {getBookmarkFromDragEvent} from './bookmark_folder.js';

interface BookmarksDragDelegate extends HTMLElement {}

class DragSession {
  private dragData_: chrome.bookmarkManagerPrivate.DragData;
  private lastPointerWasTouch_ = false;

  constructor(dragData: chrome.bookmarkManagerPrivate.DragData) {
    this.dragData_ = dragData;
  }

  start(e: DragEvent) {
    chrome.bookmarkManagerPrivate.startDrag(
        this.dragData_.elements!.map(bookmark => bookmark.id), 0,
        this.lastPointerWasTouch_, e.clientX, e.clientY);
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
    this.eventTracker_.add(this.delegate_, 'dragend', () => this.onDragEnd_());
  }

  stopObserving() {
    this.eventTracker_.removeAll();
  }

  private onDragEnd_() {
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
}