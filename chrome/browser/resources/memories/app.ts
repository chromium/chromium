// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './clusters.js';
import './router.js';
import './shared_vars.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';

import {CrToolbarElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import {CrToolbarSearchFieldElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview This file provides the root custom element for the Clusters
 * landing page.
 */

const RESULTS_PER_PAGE: number = 5;

declare global {
  interface HTMLElementTagNameMap {
    'clusters-app': HistoryClustersAppElement,
  }

  interface Window {
    // https://github.com/microsoft/TypeScript/issues/40807
    requestIdleCallback(callback: () => void): void;
  }
}

interface HistoryClustersAppElement {
  $: {
    toolbar: CrToolbarElement,
  };
}

class HistoryClustersAppElement extends PolymerElement {
  static get is() {
    return 'clusters-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The current query for which related Clusters are requested and shown.
       */
      query_: {
        type: String,
        observer: 'onQueryChanged_',
      },
    };
  }

  //============================================================================
  // Properties
  //============================================================================

  private query_: string = '';

  //============================================================================
  // Event handlers
  //============================================================================

  /**
   * Called when the value of the search field changes.
   */
  private onSearchChanged_(event: CustomEvent<string>) {
    // Update the query based on the value of the search field, if necessary.
    if (event.detail !== this.query_) {
      this.query_ = event.detail;
    }
  }

  //============================================================================
  // Helper methods
  //============================================================================

  private getSearchField_(): CrToolbarSearchFieldElement {
    return this.$.toolbar.getSearchField();
  }

  private onQueryChanged_() {
    // Update the value of the search field based on the query, if necessary.
    const searchField = this.getSearchField_();
    if (searchField.getValue() !== this.query_) {
      searchField.setValue(this.query_);
    }
  }
}

customElements.define(HistoryClustersAppElement.is, HistoryClustersAppElement);
