// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-location/iron-location.js';
import 'chrome://resources/polymer/v3_0/iron-location/iron-query-params.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {Debouncer, microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {QueryState} from './externs.js';
import {getTemplate} from './router.html.js';

// All valid pages.
// TODO(crbug.com/1473855): Change this to an enum and use that type for holding
//  these values for better type check when `loadTimeData` is no longer needed.
export const Page = {
  HISTORY: 'history',
  HISTORY_CLUSTERS: loadTimeData.getBoolean('renameJourneys') ? 'grouped' :
                                                                'journeys',
  SYNCED_TABS: 'syncedTabs',
};

// The ids of pages with corresponding tabs in the order of their tab indices.
export const TABBED_PAGES = [Page.HISTORY, Page.HISTORY_CLUSTERS];

export class HistoryRouterElement extends PolymerElement {
  static get is() {
    return 'history-router';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      selectedPage: {
        type: String,
        notify: true,
        observer: 'selectedPageChanged_',
      },

      queryState: Object,

      path_: String,

      queryParams_: Object,

      query_: {
        type: String,
        observer: 'onQueryChanged_',
      },

      urlQuery_: {
        type: String,
        observer: 'onUrlQueryChanged_',
      },
    };
  }

  static get observers() {
    return ['onUrlChanged_(path_, queryParams_)'];
  }

  selectedPage: string;
  queryState: QueryState;
  private parsing_: boolean = false;
  private debouncer_: Debouncer|null = null;
  private query_: string;
  private queryParams_: {q: string};
  private path_: string;
  private urlQuery_: string;

  override connectedCallback() {
    super.connectedCallback();

    // Redirect legacy search URLs to URLs compatible with History.
    if (window.location.hash) {
      window.location.href = window.location.href.split('#')[0] + '?' +
          window.location.hash.substr(1);
    }
  }

  /**
   * @param current Current value of the query.
   * @param previous Previous value of the query.
   */
  private onQueryChanged_(_current: string, previous?: string) {
    if (previous !== undefined) {
      this.urlQuery_ = this.query_;
    }
  }

  private onUrlQueryChanged_() {
    this.query_ = this.urlQuery_;
  }

  /**
   * Write all relevant page state to the URL.
   */
  serializeUrl() {
    let path = this.selectedPage;

    if (path === Page.HISTORY) {
      path = '';
    }

    // Make all modifications at the end of the method so observers can't change
    // the outcome.
    this.path_ = '/' + path;
    this.set('queryParams_.q', this.queryState.searchTerm || null);
  }

  private selectedPageChanged_() {
    // Update the URL if the page was changed externally, but ignore the update
    // if it came from parseUrl_().
    if (!this.parsing_) {
      this.serializeUrl();
    }
  }

  private parseUrl_() {
    this.parsing_ = true;
    const changes: {search: string} = {search: ''};
    const sections = this.path_.substr(1).split('/');
    const page = sections[0] || Page.HISTORY;

    changes.search = this.queryParams_.q || '';

    // Must change selectedPage before `change-query`, otherwise the
    // query-manager will call serializeUrl() with the old page.
    this.selectedPage = page;
    this.dispatchEvent(new CustomEvent(
        'change-query', {bubbles: true, composed: true, detail: changes}));
    this.serializeUrl();

    this.parsing_ = false;
  }

  private onUrlChanged_() {
    // Changing the url and query parameters at the same time will cause two
    // calls to onUrlChanged_. Debounce the actual work so that these two
    // changes get processed together.
    this.debouncer_ = Debouncer.debounce(
        this.debouncer_, microTask, this.parseUrl_.bind(this));
  }

  getDebouncerForTesting(): Debouncer|null {
    return this.debouncer_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'history-router': HistoryRouterElement;
  }
}

customElements.define(HistoryRouterElement.is, HistoryRouterElement);
