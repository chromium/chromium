// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {BrowserProxy} from './browser_proxy.js';
import {PageHandlerInterface} from './downloads.mojom-webui.js';

export class SearchService {
  constructor() {
    /** @private {!Array<string>} */
    this.searchTerms_ = [];

    /** @private {PageHandlerInterface} */
    this.mojoHandler_ = BrowserProxy.getInstance().handler;
  }

  /**
   * @param {string} searchText Input typed by the user into a search box.
   * @return {Array<string>} A list of terms extracted from |searchText|.
   */
  static splitTerms(searchText) {
    // Split quoted terms (e.g., 'The "lazy" dog' => ['The', 'lazy', 'dog']).
    return searchText.split(/"([^"]*)"/).map(s => s.trim()).filter(s => !!s);
  }

  /** Instructs the browser to clear all finished downloads. */
  clearAll() {
    if (loadTimeData.getBoolean('allowDeletingHistory')) {
      this.mojoHandler_.clearAll();
      this.search('');
    }
  }

  /** Loads more downloads with the current search terms. */
  loadMore() {
    this.mojoHandler_.getDownloads(this.searchTerms_);
  }

  /**
   * @return {boolean} Whether the user is currently searching for downloads
   *     (i.e. has a non-empty search term).
   */
  isSearching() {
    return this.searchTerms_.length > 0;
  }

  /**
   * @param {string} searchText What to search for.
   * @return {boolean} Whether |searchText| resulted in new search terms.
   */
  search(searchText) {
    const searchTerms = SearchService.splitTerms(searchText);
    let sameTerms = searchTerms.length === this.searchTerms_.length;

    for (let i = 0; sameTerms && i < searchTerms.length; ++i) {
      if (searchTerms[i] !== this.searchTerms_[i]) {
        sameTerms = false;
      }
    }

    if (sameTerms) {
      return false;
    }

    this.searchTerms_ = searchTerms;
    this.loadMore();
    return true;
  }
}

addSingletonGetter(SearchService);
