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
    return chrome.bookmarks.getTree();
  }

  search(query: Query) {
    return chrome.bookmarks.search(query);
  }

  update(id: string, changes: {title?: string, url?: string}) {
    return chrome.bookmarks.update(id, changes);
  }

  create(bookmark: chrome.bookmarks.CreateDetails) {
    return chrome.bookmarks.create(bookmark);
  }

  static getInstance(): BookmarksApiProxy {
    return instance || (instance = new BookmarksApiProxyImpl());
  }

  static setInstance(obj: BookmarksApiProxy) {
    instance = obj;
  }
}

let instance: BookmarksApiProxy|null = null;
