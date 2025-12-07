// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {CrRouter} from 'chrome://resources/js/cr_router.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import type {StoreObserver} from 'chrome://resources/js/store.js';

import {selectFolder, setSearchTerm} from './actions.js';
import {ROOT_NODE_ID} from './constants.js';
import {Store} from './store.js';
import type {BookmarksPageState} from './types.js';

/**
 * This element is a one way bound interface that routes the page URL to
 * the searchTerm and selectedId. Clients must initialize themselves by
 * reading the router's fields after attach.
 */
export class BookmarksRouter implements StoreObserver<BookmarksPageState> {
  private searchTerm_: string = '';
  private selectedId_: string = '';
  private defaultId_: string = '';
  private updateStateTimeout_: number|null = null;
  private tracker_: EventTracker = new EventTracker();

  initialize() {
    const store = Store.getInstance();
    store.addObserver(this);
    if (store.isInitialized()) {
      this.onStateChanged(store.data);
    }

    const router = CrRouter.getInstance();
    router.setDwellTime(200);
    this.onQueryParamsChanged_(router.getQueryParams());
    this.tracker_.add(
        router, 'cr-router-query-params-changed',
        (e: Event) => this.onQueryParamsChanged_(
            (e as CustomEvent<URLSearchParams>).detail));
  }

  teardown() {
    if (this.updateStateTimeout_) {
      clearTimeout(this.updateStateTimeout_);
      this.updateStateTimeout_ = null;
    }
    this.tracker_.removeAll();
    Store.getInstance().removeObserver(this);
  }

  private onQueryParamsChanged_(newParams: URLSearchParams) {
    const searchTerm = newParams.get('q') || '';
    let selectedId = newParams.get('id');
    if (!selectedId && !searchTerm) {
      selectedId = this.defaultId_;
    }

    if (searchTerm !== this.searchTerm_) {
      this.searchTerm_ = searchTerm;
      Store.getInstance().dispatch(setSearchTerm(searchTerm));
    }

    if (selectedId && selectedId !== this.selectedId_) {
      this.selectedId_ = selectedId;
      // Need to dispatch a deferred action so that during page load
      // `Store.getInstance().data` will only evaluate after the Store is
      // initialized.
      Store.getInstance().dispatchAsync((dispatch) => {
        dispatch(selectFolder(selectedId, Store.getInstance().data.nodes));
      });
    }
  }

  onStateChanged(state: BookmarksPageState) {
    this.selectedId_ = state.selectedFolder;
    this.searchTerm_ = state.search.term;
    // Default to the first child of root, which could be
    // ACCOUNT_HEADING_NODE_ID, BOOKMARKS_BAR_ID (local), or the id of the
    // account bookmark bar.
    this.defaultId_ = state.nodes[ROOT_NODE_ID]!.children![0]!;
    if (this.updateStateTimeout_) {
      clearTimeout(this.updateStateTimeout_);
    }
    // Debounce to microtask timing.
    this.updateStateTimeout_ = setTimeout(() => this.updateQueryParams_(), 0);
  }

  private updateQueryParams_() {
    assert(this.updateStateTimeout_);
    clearTimeout(this.updateStateTimeout_);
    this.updateStateTimeout_ = null;

    const queryParams = new URLSearchParams();
    if (this.searchTerm_) {
      queryParams.set('q', this.searchTerm_);
    } else if (this.selectedId_ !== this.defaultId_) {
      queryParams.set('id', this.selectedId_);
    }
    CrRouter.getInstance().setQueryParams(queryParams);
  }
}
