// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-location/iron-location.js';
import 'chrome://resources/polymer/v3_0/iron-location/iron-query-params.js';

import {Debouncer, microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {selectFolder, setSearchTerm} from './actions.js';
import {BOOKMARKS_BAR_ID} from './constants.js';
import {getTemplate} from './router.html.js';
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
    return getTemplate();
  }

  static get properties() {
    return {
      queryParams_: Object,

      query_: {
        type: String,
        observer: 'onQueryChanged_',
      },

      urlQuery_: {
        type: String,
        observer: 'onUrlQueryChanged_',
      },

      searchTerm_: {
        type: String,
        value: '',
      },

      selectedId_: String,
    };
  }

  private query_: string;
  private queryParams_: {q?: string, id?: string};
  private searchTerm_: string = '';
  private selectedId_: string;
  private urlQuery_: string;
  private debounceJob_: Debouncer;

  static get observers() {
    return [
      'onQueryParamsChanged_(queryParams_)',
      'onStateChanged_(searchTerm_, selectedId_)',
    ];
  }

  override connectedCallback() {
    super.connectedCallback();
    this.watch('selectedId_', state => state.selectedFolder);
    this.watch('searchTerm_', state => state.search.term);
    this.updateFromStore();
  }

  private onQueryParamsChanged_() {
    const searchTerm = this.queryParams_.q || '';
    let selectedId = this.queryParams_.id;
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

  private onQueryChanged_(_current: (string|null), previous: (string|null)) {
    if (previous !== undefined) {
      this.urlQuery_ = this.query_;
    }
  }

  private onUrlQueryChanged_() {
    this.query_ = this.urlQuery_;
  }

  private onStateChanged_() {
    this.debounceJob_ = Debouncer.debounce(
        this.debounceJob_, microTask, () => this.updateQueryParams_());
  }

  private updateQueryParams_() {
    if (this.searchTerm_) {
      this.queryParams_ = {q: this.searchTerm_};
    } else if (this.selectedId_ !== BOOKMARKS_BAR_ID) {
      this.queryParams_ = {id: this.selectedId_};
    } else {
      this.queryParams_ = {};
    }
  }
}

customElements.define(BookmarksRouterElement.is, BookmarksRouterElement);
