// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @type {?BookmarksApiProxy} */
let instance = null;

/** @interface */
export class BookmarksApiProxy {
  /** @return {!Promise<!Array<!chrome.bookmarks.BookmarkTreeNode>>} */
  getFolders() {}
}

/** @implements {BookmarksApiProxy} */
export class BookmarksApiProxyImpl {
  /** @override */
  getFolders() {
    return new Promise(resolve => chrome.bookmarks.getTree(results => {
      resolve(results[0].children);
    }));
  }

  static getInstance() {
    return instance || (instance = new BookmarksApiProxyImpl());
  }

  /** @param {!BookmarksApiProxy} obj */
  static setInstance(obj) {
    instance = obj;
  }
}