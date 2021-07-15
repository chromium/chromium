// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-location/iron-location.js';
import 'chrome://resources/polymer/v3_0/iron-location/iron-query-params.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview This file provides a custom element that routes the query
 * parameters of the page URL to the search query.
 */

declare global {
  interface HTMLElementTagNameMap {
    'clusters-router': ClustersRouterElement,
  }
}

class ClustersRouterElement extends PolymerElement {
  static get is() {
    return 'clusters-router';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The value of the query parameter with the name q.
       */
      query: {
        type: String,
        notify: true,
        observer: 'onQueryChanged_',
      },

      /**
       * The parsed query parameters of the page URL ({foo: 'bar'}).
       */
      queryParamsObject_: {
        type: Object,
        observer: 'onQueryParamsObjectChanged_',
      },

      /**
       * The string query parameters of the page URL ('?foo=bar'), provided by
       * <iron-location> and parsed by <iron-query-params> into
       * `queryParamsObject_` and vice versa. Not to be modified directly.
       */
      queryParamsString_: {
        type: String,
      },
    };
  }

  //============================================================================
  // Properties
  //============================================================================

  query: string = '';
  private queryParamsObject_: {q: string}|null = null;
  private queryParamsString_: string = '';

  //============================================================================
  // Helper methods
  //============================================================================

  private onQueryChanged_() {
    this.queryParamsObject_ = this.query ? {q: this.query} : null;
  }

  private onQueryParamsObjectChanged_() {
    this.query = this.queryParamsObject_ ? this.queryParamsObject_.q : '';
  }
}

customElements.define(ClustersRouterElement.is, ClustersRouterElement);
