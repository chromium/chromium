// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {QueryState} from 'chrome://resources/cr_components/history/history.mojom-webui.js';
import {CrRouter} from 'chrome://resources/js/cr_router.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

// All valid pages.
// TODO(crbug.com/40069898): Change this to an enum and use that type for holding
//  these values for better type check when `loadTimeData` is no longer needed.
export const Page = {
  HISTORY: 'history',
  HISTORY_CLUSTERS: 'grouped',
  SYNCED_TABS: 'syncedTabs',
  PRODUCT_SPECIFICATIONS_LISTS: 'comparisonTables',
};

// The ids of pages with corresponding tabs in the order of their tab indices.
export const TABBED_PAGES = [Page.HISTORY, Page.HISTORY_CLUSTERS];

export class HistoryRouterElement extends CrLitElement {
  static get is() {
    return 'history-router';
  }

  static get template() {
    return null;
  }

  static override get properties() {
    return {
      lastSelectedTab: {
        type: Number,
      },
      selectedPage: {
        type: String,
        notify: true,
      },

      queryState: {
        type: Object,
      },
    };
  }

  accessor lastSelectedTab: number;
  accessor selectedPage: string;
  accessor queryState: QueryState;
  timeRangeStart?: Date;

  private eventTracker_: EventTracker = new EventTracker();

  override connectedCallback() {
    super.connectedCallback();

    // Redirect legacy search URLs to URLs compatible with History.
    if (window.location.hash) {
      window.location.href = window.location.href.split('#')[0] + '?' +
          window.location.hash.substr(1);
    }

    const router = CrRouter.getInstance();
    this.onPathChanged_(router.getPath());
    this.onQueryParamsChanged_(router.getQueryParams());
    this.eventTracker_.add(
        router, 'cr-router-path-changed',
        (e: Event) => this.onPathChanged_((e as CustomEvent<string>).detail));
    this.eventTracker_.add(
        router, 'cr-router-query-params-changed',
        (e: Event) => this.onQueryParamsChanged_(
            (e as CustomEvent<URLSearchParams>).detail));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if ((changedProperties.has('queryState') &&
         changedProperties.get('queryState')) ||
        (changedProperties.has('selectedPage') &&
         changedProperties.get('selectedPage'))) {
      this.serializeUrl();
    }
  }

  /**
   * Write all relevant page state to the URL.
   */
  serializeUrl() {
    let path = this.selectedPage;

    if (path === Page.HISTORY) {
      path = '';
    }

    const router = CrRouter.getInstance();
    router.setPath('/' + path);

    if (!this.queryState) {
      return;
    }
    const queryParams = new URLSearchParams();
    if (this.queryState.searchTerm) {
      queryParams.set('q', this.queryState.searchTerm);
    }
    if (this.queryState.after) {
      queryParams.set('after', this.queryState.after);
    }
    router.setQueryParams(queryParams);
  }

  private onPathChanged_(newPath: string) {
    const sections = newPath.substr(1).split('/');
    const page = sections[0] ||
        (window.location.search ? 'history' :
                                  TABBED_PAGES[this.lastSelectedTab]);
    // TODO(b/338245900): This is kind of nasty. Without cr-tabs to constrain
    //   `selectedPage`, this can be set to an arbitrary value from the URL.
    //   To fix this, we should constrain the selected pages to an actual enum.
    this.selectedPage = page;
  }

  private onQueryParamsChanged_(newParams: URLSearchParams) {
    const changes: {search: string, after?: string} = {search: ''};
    changes.search = newParams.get('q') || '';
    let after = '';
    const afterFromParams = newParams.get('after');
    if (!!afterFromParams && afterFromParams.match(/^\d{4}-\d{2}-\d{2}$/)) {
      const afterAsDate = new Date(afterFromParams);
      if (!isNaN(afterAsDate.getTime())) {
        after = afterFromParams;
      }
    }
    changes.after = after;
    this.dispatchEvent(new CustomEvent(
        'change-query', {bubbles: true, composed: true, detail: changes}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'history-router': HistoryRouterElement;
  }
}

customElements.define(HistoryRouterElement.is, HistoryRouterElement);
