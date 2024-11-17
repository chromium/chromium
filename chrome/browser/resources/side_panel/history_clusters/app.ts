// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/cr_components/history_clusters/browser_proxy.js';
import 'chrome://resources/cr_components/history_clusters/clusters.js';
import 'chrome://resources/cr_components/history_embeddings/history_embeddings.js';
import 'chrome://resources/cr_components/history_embeddings/icons.html.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {BrowserProxyImpl} from 'chrome://resources/cr_components/history_clusters/browser_proxy.js';
import type {HistoryClustersElement} from 'chrome://resources/cr_components/history_clusters/clusters.js';
import {HistoryEmbeddingsBrowserProxyImpl} from 'chrome://resources/cr_components/history_embeddings/browser_proxy.js';
import type {HistoryEmbeddingsMoreActionsClickEvent, HistoryEmbeddingsResultClickEvent, HistoryEmbeddingsResultContextMenuEvent} from 'chrome://resources/cr_components/history_embeddings/history_embeddings.js';
import type {CrToolbarSearchFieldElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

export interface HistoryClustersAppElement {
  $: {
    embeddingsScrollContainer: HTMLElement,
    searchbox: CrToolbarSearchFieldElement,
    historyClusters: HistoryClustersElement,
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
      enableHistoryEmbeddings_: {type: Boolean, reflect: true},

      hasHistoryEmbeddingsResults_: {type: Boolean, reflect: true},

      historyEmbeddingsDisclaimerLinkClicked_: {type: Boolean},

      nonEmbeddingsResultClicked_: {type: Boolean},

      numCharsTypedInSearch_: {type: Number},

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

  protected enableHistoryEmbeddings_ =
      loadTimeData.getBoolean('enableHistoryEmbeddings');
  protected hasHistoryEmbeddingsResults_ = false;
  protected historyEmbeddingsDisclaimerLinkClicked_ = false;
  query: string = '';
  protected nonEmbeddingsResultClicked_: boolean = false;
  protected numCharsTypedInSearch_: number = 0;
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
    this.scrollTarget_ = this.enableHistoryEmbeddings_ ?
        this.$.embeddingsScrollContainer :
        this.$.historyClusters;

    if (this.enableHistoryEmbeddings_) {
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

  protected getClustersComponentClass_(): string {
    return this.enableHistoryEmbeddings_ ?
        '' :
        'sp-scroller sp-scroller-bottom-of-page';
  }

  protected onHistoryEmbeddingsDisclaimerLinkClick_(e: Event) {
    e.preventDefault();
    this.historyEmbeddingsDisclaimerLinkClicked_ = true;
    HistoryEmbeddingsBrowserProxyImpl.getInstance().openSettingsPage();
  }

  /**
   * Called when the value of the search field changes.
   */
  protected onSearchChanged_(event: CustomEvent<string>) {
    // Update the query based on the value of the search field, if necessary.
    this.query = event.detail;

    this.nonEmbeddingsResultClicked_ = false;
  }

  protected onHistoryEmbeddingsResultClick_(
      event: HistoryEmbeddingsResultClickEvent) {
    BrowserProxyImpl.getInstance().handler.openHistoryUrl(
        event.detail.item.url, {
          middleButton: event.detail.middleButton,
          altKey: event.detail.altKey,
          ctrlKey: event.detail.ctrlKey,
          metaKey: event.detail.metaKey,
          shiftKey: event.detail.shiftKey,
        });
  }

  protected onHistoryEmbeddingsResultContextMenu_(
      event: HistoryEmbeddingsResultContextMenuEvent) {
    event.preventDefault();
    BrowserProxyImpl.getInstance().handler.showContextMenuForURL(
        event.detail.item.url, {
          x: event.detail.x,
          y: event.detail.y,
        });
  }

  protected onHistoryEmbeddingsIsEmptyChanged_(
      e: CustomEvent<{value: boolean}>) {
    this.hasHistoryEmbeddingsResults_ = !e.detail.value;
  }

  protected onHistoryEmbeddingsItemRemoveClick_(
      e: HistoryEmbeddingsMoreActionsClickEvent) {
    e.preventDefault();
    const historyEmbeddingsItem = e.detail;
    BrowserProxyImpl.getInstance().handler.removeVisitByUrlAndTime(
        {url: historyEmbeddingsItem.url.url},
        historyEmbeddingsItem.lastUrlVisitTimestamp);
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

  protected onClusterLinkClick_() {
    this.nonEmbeddingsResultClicked_ = true;
  }

  protected onSearchCleared_() {
    if (!this.enableHistoryEmbeddings_) {
      return;
    }

    this.numCharsTypedInSearch_ = 0;
  }

  protected onSearchNativeInput_(
      e: CustomEvent<{e: InputEvent, inputValue: string}>) {
    if (!this.enableHistoryEmbeddings_) {
      return;
    }

    if (e.detail.inputValue.length === 0) {
      // Input was cleared (eg. backspace/delete was hit).
      this.numCharsTypedInSearch_ = 0;
    } else if (e.detail.e.data === e.detail.inputValue) {
      // If the inserted text matches exactly with the current value of the
      // input, that implies that the previous input value was cleared or
      // was empty to begin with. So, reset the num chars typed and count this
      // input event as 1 char typed.
      this.numCharsTypedInSearch_ = 1;
    } else {
      this.numCharsTypedInSearch_++;
    }
  }

  protected shouldShowHistoryEmbeddingsResults_(): boolean {
    if (!this.enableHistoryEmbeddings_) {
      return false;
    }

    if (!this.query) {
      return false;
    }

    return this.query.split(' ').filter(part => part.length > 0).length >=
        loadTimeData.getInteger('historyEmbeddingsSearchMinimumWordCount');
  }
}
declare global {
  interface HTMLElementTagNameMap {
    'history-clusters-app': HistoryClustersAppElement;
  }
}
customElements.define(HistoryClustersAppElement.is, HistoryClustersAppElement);
