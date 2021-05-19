// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserService} from './browser_service.js';
import {HistoryEntry, HistoryQuery, QueryResult} from './externs.js';
import {QueryState} from './externs.js';
import {HistoryRouterElement} from './router.js';

/** @polymer */
export class HistoryQueryManagerElement extends PolymerElement {
  static get is() {
    return 'history-query-manager';
  }

  static get template() {
    return null;
  }

  static get properties() {
    return {
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
    };
  }

  static get observers() {
    return ['searchTermChanged_(queryState.searchTerm)'];
  }

  constructor() {
    super();

    /** @private {!EventTracker} */
    this.eventTracker_ = new EventTracker();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.eventTracker_.add(
        document, 'change-query', this.onChangeQuery_.bind(this));
    this.eventTracker_.add(
        document, 'query-history', this.onQueryHistory_.bind(this));
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  initialize() {
    this.queryHistory_(false /* incremental */);
  }

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
  }

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
  }

  /**
   * @param {!Event} e
   * @private
   */
  onQueryHistory_(e) {
    this.queryHistory_(/** @type {boolean} */ (e.detail));
    return false;
  }

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
    this.dispatchEvent(
        new CustomEvent('query-finished', {bubbles: true, composed: true}));
  }

  /** @private */
  searchTermChanged_() {
    // TODO(tsergeant): Ignore incremental searches in this metric.
    if (this.queryState.searchTerm) {
      BrowserService.getInstance().recordAction('Search');
    }
  }
}

customElements.define(
    HistoryQueryManagerElement.is, HistoryQueryManagerElement);
