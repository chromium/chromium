// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shared_style.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SearchQuery} from './history_clusters.mojom-webui.js';

/**
 * @fileoverview This file provides a custom element displaying a search query.
 */

declare global {
  interface HTMLElementTagNameMap {
    'search-query': SearchQueryElement,
  }
}

class SearchQueryElement extends PolymerElement {
  static get is() {
    return 'search-query';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The search query to display.
       */
      searchQuery: Object,
    };
  }

  //============================================================================
  // Properties
  //============================================================================

  searchQuery: SearchQuery = new SearchQuery();
}

customElements.define(SearchQueryElement.is, SearchQueryElement);
