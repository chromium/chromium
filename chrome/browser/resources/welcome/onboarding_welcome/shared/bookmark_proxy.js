// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('nux', function() {
  /**
   * @typedef {{
   *    parentId: string,
   *    title: string,
   *    url: string,
   * }}
   */
  let bookmarkData;

  /** @interface */
  class BookmarkProxy {
    /**
     * @param {!bookmarkData} data
     * @param {!Function} callback
     */
    addBookmark(data, callback) {}

    /** @param {string} id ID provided by callback when bookmark was added. */
    removeBookmark(id) {}

    /** @param {boolean} show */
    toggleBookmarkBar(show) {}
  }

  /** @implements {nux.BookmarkProxy} */
  class BookmarkProxyImpl {
    /** @override */
    addBookmark(data, callback) {
      chrome.bookmarks.create(data, callback);
    }

    /** @override */
    removeBookmark(id) {
      chrome.bookmarks.remove(id);
    }

    /** @override */
    toggleBookmarkBar(show) {
      chrome.send('toggleBookmarkBar', [show]);
    }
  }

  cr.addSingletonGetter(BookmarkProxyImpl);

  return {
    BookmarkProxy: BookmarkProxy,
    BookmarkProxyImpl: BookmarkProxyImpl,
  };
});
