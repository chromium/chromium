// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shared_style.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SearchQuery} from './history_clusters.mojom-webui.js';
import {MetricsProxyImpl, RelatedSearchAction} from './metrics_proxy.js';
import {OpenWindowProxyImpl} from './open_window_proxy.js';
import {getTemplate} from './search_query.html.js';

/**
 * @fileoverview This file provides a custom element displaying a search query.
 */

declare global {
  interface HTMLElementTagNameMap {
    'search-query': SearchQueryElement;
  }
}

class SearchQueryElement extends PolymerElement {
  static get is() {
    return 'search-query';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The index of the search query pill.
       */
      index: {
        type: Number,
        value: -1,  // Initialized to an invalid value.
      },

      /**
       * The search query to display.
       */
      searchQuery: Object,
    };
  }

  //============================================================================
  // Properties
  //============================================================================

  index: number;
  searchQuery: SearchQuery;

  //============================================================================
  // Event handlers
  //============================================================================

  private onAuxClick_() {
    MetricsProxyImpl.getInstance().recordRelatedSearchAction(
        RelatedSearchAction.CLICKED, this.index);

    // Notify the parent <history-cluster> element of this event.
    this.dispatchEvent(new CustomEvent('related-search-clicked', {
      bubbles: true,
      composed: true,
    }));
  }

  private onClick_(event: MouseEvent) {
    event.preventDefault();  // Prevent default browser action (navigation).

    // To record metrics.
    this.onAuxClick_();

    OpenWindowProxyImpl.getInstance().open(this.searchQuery.url.url);
  }

  private onKeydown_(e: KeyboardEvent) {
    // To be consistent with <history-list>, only handle Enter, and not Space.
    if (e.key !== 'Enter') {
      return;
    }

    // To record metrics.
    this.onAuxClick_();

    OpenWindowProxyImpl.getInstance().open(this.searchQuery.url.url);
  }
}

customElements.define(SearchQueryElement.is, SearchQueryElement);
