// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export type Query = string|{
  query?: string,
  url?: string,
  title?: string,
};

export interface BookmarksApiProxy {
  getTree(): Promise<chrome.bookmarks.BookmarkTreeNode[]>;
  search(query: Query): Promise<chrome.bookmarks.BookmarkTreeNode[]>;
  update(id: string, changes: {title?: string, url?: string}):
      Promise<chrome.bookmarks.BookmarkTreeNode>;
  create(bookmark: chrome.bookmarks.CreateDetails):
      Promise<chrome.bookmarks.BookmarkTreeNode>;
}

export class BookmarksApiProxyImpl implements BookmarksApiProxy {
  getTree() {
    return new Promise<chrome.bookmarks.BookmarkTreeNode[]>(resolve => {
      chrome.bookmarks.getTree(resolve);
    });
  }

  search(query: Query) {
    return new Promise<chrome.bookmarks.BookmarkTreeNode[]>(resolve => {
      chrome.bookmarks.search(query, resolve);
    });
  }

  update(id: string, changes: {title?: string, url?: string}) {
    return new Promise<chrome.bookmarks.BookmarkTreeNode>(resolve => {
      chrome.bookmarks.update(id, changes, resolve);
    });
  }

  create(bookmark: chrome.bookmarks.CreateDetails) {
    return new Promise<chrome.bookmarks.BookmarkTreeNode>(resolve => {
      chrome.bookmarks.create(bookmark, resolve);
    });
  }

  static getInstance(): BookmarksApiProxy {
    return instance || (instance = new BookmarksApiProxyImpl());
  }

  static setInstance(obj: BookmarksApiProxy) {
    instance = obj;
  }
}

let instance: BookmarksApiProxy|null = null;
