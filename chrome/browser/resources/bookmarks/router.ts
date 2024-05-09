// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrRouter} from 'chrome://resources/js/cr_router.js';
import {Debouncer, microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {selectFolder, setSearchTerm} from './actions.js';
import {BOOKMARKS_BAR_ID} from './constants.js';
import {StoreClientMixin} from './store_client_mixin.js';

const BookmarksRouterElementBase = StoreClientMixin(PolymerElement);

/**
 * This element is a one way bound interface that routes the page URL to
 * the searchTerm and selectedId. Clients must initialize themselves by
 * reading the router's fields after attach.
 */
export class BookmarksRouterElement extends BookmarksRouterElementBase {
  static get is() {
    return 'bookmarks-router';
  }

  static get template() {
    return null;
  }

  static get properties() {
    return {
      searchTerm_: {
        type: String,
        value: '',
      },

      selectedId_: String,
    };
  }

  private searchTerm_: string = '';
  private selectedId_: string;
  private debounceJob_: Debouncer;
  private queryListener_: EventListener|null = null;

  static get observers() {
    return [
      'onStateChanged_(searchTerm_, selectedId_)',
    ];
  }

  override connectedCallback() {
    super.connectedCallback();
    this.watch('selectedId_', state => state.selectedFolder);
    this.watch('searchTerm_', state => state.search.term);
    this.updateFromStore();

    const router = CrRouter.getInstance();
    router.setDwellTime(200);
    this.onQueryParamsChanged_(router.getQueryParams());
    this.queryListener_ = (e: Event) =>
        this.onQueryParamsChanged_((e as CustomEvent<URLSearchParams>).detail);
    router.addEventListener(
        'cr-router-query-params-changed', this.queryListener_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    CrRouter.getInstance().removeEventListener(
        'cr-router-query-params-changed', this.queryListener_);
    this.queryListener_ = null;
  }

  private onQueryParamsChanged_(newParams: URLSearchParams) {
    const searchTerm = newParams.get('q') || '';
    let selectedId = newParams.get('id');
    if (!selectedId && !searchTerm) {
      selectedId = BOOKMARKS_BAR_ID;
    }

    if (searchTerm !== this.searchTerm_) {
      this.searchTerm_ = searchTerm;
      this.dispatch(setSearchTerm(searchTerm));
    }

    if (selectedId && selectedId !== this.selectedId_) {
      this.selectedId_ = selectedId;
      // Need to dispatch a deferred action so that during page load
      // `this.getState()` will only evaluate after the Store is initialized.
      this.dispatchAsync((dispatch) => {
        dispatch(selectFolder(selectedId!, this.getState().nodes));
      });
    }
  }

  private onStateChanged_() {
    this.debounceJob_ = Debouncer.debounce(
        this.debounceJob_, microTask, () => this.updateQueryParams_());
  }

  private updateQueryParams_() {
    const queryParams = new URLSearchParams();
    if (this.searchTerm_) {
      queryParams.set('q', this.searchTerm_);
    } else if (this.selectedId_ !== BOOKMARKS_BAR_ID) {
      queryParams.set('id', this.selectedId_);
    }
    CrRouter.getInstance().setQueryParams(queryParams);
  }
}

customElements.define(BookmarksRouterElement.is, BookmarksRouterElement);
