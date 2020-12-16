// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserService} from './browser_service.js';
import {HistoryEntry, HistoryQuery, QueryResult} from './externs.js';
import {QueryState} from './externs.js';

Polymer({
  is: 'history-query-manager',

  properties: {
    /** @type {QueryState} */
    queryState: {
      type: Object,
      notify: true,
      value() {
        return {
          // Whether the most recent query was incremental.
          incremental: false,
          // A query is initiated by page load.
          querying: true,
          searchTerm: '',
        };
      },
    },

    /** @type {QueryResult} */
    queryResult: {
      type: Object,
      notify: true,
    },

    /** @type {?HistoryRouterElement} */
    router: Object,
  },

  observers: [
    'searchTermChanged_(queryState.searchTerm)',
  ],

  /** @private {!Object<string, !function(!Event)>} */
  documentListeners_: {},

  /** @override */
  attached() {
    this.documentListeners_['change-query'] = this.onChangeQuery_.bind(this);
    this.documentListeners_['query-history'] = this.onQueryHistory_.bind(this);

    for (const e in this.documentListeners_) {
      document.addEventListener(e, this.documentListeners_[e]);
    }
  },

  /** @override */
  detached() {
    for (const e in this.documentListeners_) {
      document.removeEventListener(e, this.documentListeners_[e]);
    }
  },

  initialize() {
    this.queryHistory_(false /* incremental */);
  },

  /**
   * @param {boolean} incremental
   * @private
   */
  queryHistory_(incremental) {
    this.set('queryState.querying', true);
    this.set('queryState.incremental', incremental);

    const browserService = BrowserService.getInstance();
    const promise = incremental ?
        browserService.queryHistoryContinuation() :
        browserService.queryHistory(this.queryState.searchTerm);
    // Ignore rejected (cancelled) queries.
    promise.then(result => this.onQueryResult_(result), () => {});
  },

  /**
   * @param {!Event} e
   * @private
   */
  onChangeQuery_(e) {
    const changes = /** @type {{search: ?string}} */ (e.detail);
    let needsUpdate = false;

    if (changes.search !== null &&
        changes.search !== this.queryState.searchTerm) {
      this.set('queryState.searchTerm', changes.search);
      needsUpdate = true;
    }

    if (needsUpdate) {
      this.queryHistory_(false);
      if (this.router) {
        this.router.serializeUrl();
      }
    }
  },

  /**
   * @param {!Event} e
   * @private
   */
  onQueryHistory_(e) {
    this.queryHistory_(/** @type {boolean} */ (e.detail));
    return false;
  },

  /**
   * @param {{info: !HistoryQuery,
   *          value: !Array<!HistoryEntry>}} results List of results with
   *     information about the query.
   * @private
   */
  onQueryResult_(results) {
    this.set('queryState.querying', false);
    this.set('queryResult.info', results.info);
    this.set('queryResult.results', results.value);
    this.fire('query-finished');
  },

  /** @private */
  searchTermChanged_() {
    // TODO(tsergeant): Ignore incremental searches in this metric.
    if (this.queryState.searchTerm) {
      BrowserService.getInstance().recordAction('Search');
    }
  },
});
