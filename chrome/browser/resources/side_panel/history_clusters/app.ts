// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import 'chrome://resources/cr_components/history_clusters/browser_proxy.js';
import 'chrome://resources/cr_components/history_clusters/clusters.js';
import 'chrome://resources/cr_components/history_embeddings/icons.html.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {BrowserProxyImpl} from 'chrome://resources/cr_components/history_clusters/browser_proxy.js';
import type {CrToolbarSearchFieldElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

export interface HistoryClustersAppElement {
  $: {
    searchbox: CrToolbarSearchFieldElement,
    historyClusters: HTMLElement,
  };
}

export class HistoryClustersAppElement extends CrLitElement {
  static get is() {
    return 'history-clusters-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * The current query for which related clusters are requested and shown.
       */
      query: {type: String},

      scrollTarget_: {type: Object},

      searchIcon_: {type: String},
    };
  }

  constructor() {
    super();
    ColorChangeUpdater.forDocument().start();
  }

  //============================================================================
  // Properties
  //============================================================================

  query: string = '';
  protected scrollTarget_?: HTMLElement;
  protected searchIcon_?: string;

  //============================================================================
  // Event Handlers
  //============================================================================

  protected onContextMenu_(event: MouseEvent) {
    BrowserProxyImpl.getInstance().handler.showContextMenuForSearchbox(
        this.query, {x: event.clientX, y: event.clientY});
  }

  //============================================================================
  // Overridden methods
  //============================================================================

  override connectedCallback() {
    super.connectedCallback();
    this.scrollTarget_ = this.$.historyClusters;

    if (loadTimeData.getBoolean('enableHistoryEmbeddings')) {
      this.searchIcon_ = 'history-embeddings:search';
    }

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
  protected onSearchChanged_(event: CustomEvent<string>) {
    // Update the query based on the value of the search field, if necessary.
    this.query = event.detail;
  }

  /**
   * Called when the browser handler forces us to change our query.
   */
  protected onQueryChangedByUser_(event: CustomEvent<string>) {
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
