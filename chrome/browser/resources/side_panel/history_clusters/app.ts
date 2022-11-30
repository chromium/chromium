// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';
import 'chrome://resources/cr_components/history_clusters/clusters.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';

import {CrToolbarSearchFieldElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';

export interface HistoryClustersAppElement {
  $: {
    searchbox: CrToolbarSearchFieldElement,
  };
}

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

  //============================================================================
  // Overridden methods
  //============================================================================

  override connectedCallback() {
    super.connectedCallback();

    // Populate the initial query from the URL parameter. Other methods are
    // mostly racy.
    const initialQuery =
        new URLSearchParams(window.location.search).get('initial_query');
    if (initialQuery) {
      this.$.searchbox.setValue(initialQuery);
    }
  }

  /**
   * Called when the value of the search field changes.
   */
  private onSearchChanged_(event: CustomEvent<string>) {
    // Update the query based on the value of the search field, if necessary.
    this.query = event.detail;
  }

  /**
   * Called when the browser handler forces us to change our query.
   */
  private onQueryChangedByUser_(event: CustomEvent<string>) {
    // This will in turn fire `onSearchChanged_()`.
    if (this.$.searchbox) {
      this.$.searchbox.setValue(event.detail);
    }
  }
}
declare global {
  interface HTMLElementTagNameMap {
    'history-clusters-app': HistoryClustersAppElement;
  }
}
customElements.define(HistoryClustersAppElement.is, HistoryClustersAppElement);