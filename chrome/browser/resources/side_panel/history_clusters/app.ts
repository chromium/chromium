// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';
import 'chrome://resources/cr_components/history_clusters/clusters.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';

export class HistoryClustersAppElement extends PolymerElement {
  static get is() {
    return 'history-clusters-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The current query for which related clusters are requested and shown.
       */
      query: {
        type: String,
        value: '',
      },
    };
  }

  //============================================================================
  // Properties
  //============================================================================

  query: string;

  /**
   * Called when the value of the search field changes.
   */
  private onSearchChanged_(event: CustomEvent<string>) {
    // Update the query based on the value of the search field, if necessary.
    this.query = event.detail;
  }
}
declare global {
  interface HTMLElementTagNameMap {
    'history-clusters-app': HistoryClustersAppElement;
  }
}
customElements.define(HistoryClustersAppElement.is, HistoryClustersAppElement);