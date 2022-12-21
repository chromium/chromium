// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

export interface BookmarkData {
  parentId: string;
  title: string;
  url: string;
}

export type AddBookmarkCallback = (node: chrome.bookmarks.BookmarkTreeNode) =>
    void;

export interface BookmarkProxy {
  addBookmark(data: BookmarkData): Promise<chrome.bookmarks.BookmarkTreeNode>;

  /** @param id ID provided by callback when bookmark was added. */
  removeBookmark(id: string): void;

  toggleBookmarkBar(show: boolean): void;
  isBookmarkBarShown(): Promise<boolean>;
}

export class BookmarkProxyImpl implements BookmarkProxy {
  addBookmark(data: BookmarkData) {
    return chrome.bookmarks.create(data);
  }

  removeBookmark(id: string) {
    chrome.bookmarks.remove(id);
  }

  toggleBookmarkBar(show: boolean) {
    chrome.send('toggleBookmarkBar', [show]);
  }

  isBookmarkBarShown() {
    return sendWithPromise('isBookmarkBarShown');
  }

  static getInstance(): BookmarkProxy {
    return bookmarkProxyInstance ||
        (bookmarkProxyInstance = new BookmarkProxyImpl());
  }

  static setInstance(obj: BookmarkProxy) {
    bookmarkProxyInstance = obj;
  }
}

let bookmarkProxyInstance: BookmarkProxy|null = null;

// Wrapper for bookmark proxy to keep some additional states.
export class BookmarkBarManager {
  private proxy_: BookmarkProxy;
  private isBarShown_: boolean = false;
  initialized: Promise<void>;

  constructor() {
    this.proxy_ = BookmarkProxyImpl.getInstance();
    this.initialized = this.proxy_.isBookmarkBarShown().then((shown) => {
      this.isBarShown_ = shown;
    });
  }

  getShown(): boolean {
    return this.isBarShown_;
  }

  setShown(show: boolean) {
    this.isBarShown_ = show;
    this.proxy_.toggleBookmarkBar(show);
  }

  static getInstance(): BookmarkBarManager {
    return managerInstance || (managerInstance = new BookmarkBarManager());
  }

  static setInstance(obj: BookmarkBarManager) {
    managerInstance = obj;
  }
}

let managerInstance: BookmarkBarManager|null = null;
