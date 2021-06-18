// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ChromeEvent} from '/tools/typescript/definitions/chrome_event.js';

let instance: BookmarksApiProxy|null = null;

export class BookmarksApiProxy {
  callbackRouter: {[key: string]: ChromeEvent<Function>};

  constructor() {
    this.callbackRouter = {
      onChanged: chrome.bookmarks.onChanged,
      onChildrenReordered: chrome.bookmarks.onChildrenReordered,
      onCreated: chrome.bookmarks.onCreated,
      onMoved: chrome.bookmarks.onMoved,
      onRemoved: chrome.bookmarks.onRemoved,
    };
  }

  getFolders(): Promise<chrome.bookmarks.BookmarkTreeNode[]> {
    return new Promise(resolve => chrome.bookmarks.getTree(results => {
      if (results[0] && results[0].children) {
        resolve(results[0].children);
        return;
      }
      resolve([]);
    }));
  }

  static getInstance() {
    return instance || (instance = new BookmarksApiProxy());
  }

  static setInstance(obj: BookmarksApiProxy) {
    instance = obj;
  }
}