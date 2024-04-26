// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserServiceImpl} from './browser_service.js';
import type {HistoryEntry, HistoryQuery, QueryResult, QueryState} from './externs.js';
import type {HistoryRouterElement} from './router.js';

// Converts a JS Date object to a human readable string in the format of
// YYYY-MM-DD for the query.
export function convertDateToQueryValue(date: Date) {
  const fullYear = date.getFullYear();
  const month = date.getMonth() + 1; /* Month is 0-indexed. */
  const day = date.getDate();

  function twoDigits(value: number): string {
    return value >= 10 ? `${value}` : `0${value}`;
  }

  return `${fullYear}-${twoDigits(month)}-${twoDigits(day)}`;
}

declare global {
  interface HTMLElementTagNameMap {
    'history-query-manager': HistoryQueryManagerElement;
  }
}

export class HistoryQueryManagerElement extends PolymerElement {
  static get is() {
    return 'history-query-manager';
  }

  static get template() {
    return null;
  }

  static get properties() {
    return {
      queryState: {
        type: Object,
        notify: true,
      },

      queryResult: {
        type: Object,
        notify: true,
      },

      router: Object,
    };
  }

  static get observers() {
    return ['searchTermChanged_(queryState.searchTerm)'];
  }

  private eventTracker_: EventTracker = new EventTracker();
  queryState: QueryState;
  queryResult: QueryResult;
  router?: HistoryRouterElement;

  constructor() {
    super();

    this.queryState = {
      // Whether the most recent query was incremental.
      incremental: false,
      // A query is initiated by page load.
      querying: true,
      searchTerm: '',
    };
  }

  override connectedCallback() {
    super.connectedCallback();
    this.eventTracker_.add(
        document, 'change-query', this.onChangeQuery_.bind(this));
    this.eventTracker_.add(
        document, 'query-history', this.onQueryHistory_.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  initialize() {
    this.queryHistory_(false /* incremental */);
  }

  private queryHistory_(incremental: boolean) {
    this.set('queryState.querying', true);
    this.set('queryState.incremental', incremental);

    let afterTimestamp;
    if (loadTimeData.getBoolean('enableHistoryEmbeddings') &&
        this.queryState.after) {
      const afterDate = new Date(this.queryState.after);
      afterDate.setHours(0, 0, 0, 0);
      afterTimestamp = afterDate.getTime();
    }

    const browserService = BrowserServiceImpl.getInstance();
    const promise = incremental ?
        browserService.queryHistoryContinuation() :
        browserService.queryHistory(this.queryState.searchTerm, afterTimestamp);
    // Ignore rejected (cancelled) queries.
    promise.then(result => this.onQueryResult_(result), () => {});
  }

  private onChangeQuery_(e: CustomEvent<{search?: string, after?: string}>) {
    const changes = e.detail;
    let needsUpdate = false;

    if (changes.search !== null &&
        changes.search !== this.queryState.searchTerm) {
      this.set('queryState.searchTerm', changes.search);
      needsUpdate = true;
    }

    if (loadTimeData.getBoolean('enableHistoryEmbeddings') &&
        changes.after !== null && changes.after !== this.queryState.after &&
        (Boolean(changes.after) || Boolean(this.queryState.after))) {
      this.set('queryState.after', changes.after);
      needsUpdate = true;
    }

    if (needsUpdate) {
      this.queryHistory_(false);
      if (this.router) {
        this.router.serializeUrl();
      }
    }
  }

  private onQueryHistory_(e: CustomEvent<boolean>): boolean {
    this.queryHistory_(e.detail);
    return false;
  }

  /**
   * @param results List of results with information about the query.
   */
  private onQueryResult_(results: {info: HistoryQuery, value: HistoryEntry[]}) {
    this.set('queryState.querying', false);
    this.set('queryResult.info', results.info);
    this.set('queryResult.results', results.value);
    this.dispatchEvent(
        new CustomEvent('query-finished', {bubbles: true, composed: true}));
  }

  private searchTermChanged_() {
    // TODO(tsergeant): Ignore incremental searches in this metric.
    if (this.queryState.searchTerm) {
      BrowserServiceImpl.getInstance().recordAction('Search');
    }
  }
}

customElements.define(
    HistoryQueryManagerElement.is, HistoryQueryManagerElement);
