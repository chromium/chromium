// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cluster.js';
import './router.js';
import './shared_style.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/iron-scroll-threshold/iron-scroll-threshold.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import {CrToolbarElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import {CrToolbarSearchFieldElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {IronScrollThresholdElement} from 'chrome://resources/polymer/v3_0/iron-scroll-threshold/iron-scroll-threshold.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';
import {PageCallbackRouter, PageHandlerRemote, QueryParams, QueryResult} from './chrome/browser/ui/webui/history_clusters/history_clusters.mojom-webui.js';
import {URLVisit} from './components/history_clusters/core/history_clusters.mojom-webui.js';

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
    confirmationDialog: CrLazyRenderElement<CrDialogElement>,
    scrollThreshold: IronScrollThresholdElement,
    toolbar: CrToolbarElement,
    container: Element,
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

      /**
       * Contains 1) the Clusters returned by the browser in response to a
       * request for the freshest Clusters related to a given query until a
       * given time threshold and 2) the optional continuation query parameters
       * returned alongside the Clusters to be used in the follow-up request to
       * load older Clusters.
       */
      result_: Object,

      /**
       * The title to show when the query is non-empty.
       */
      title_: {
        type: String,
        computed: `computeTitle_(result_)`,
      },

      /**
       * The list of visits to be removed. A non-empty array indicates a pending
       * remove request to the browser.
       */
      visitsToBeRemoved_: {
        type: Object,
        value: [],
      },
    };
  }

  //============================================================================
  // Properties
  //============================================================================

  private callbackRouter_: PageCallbackRouter;
  private onClustersQueryResultListenerId_: number|null = null;
  private onVisitsRemovedListenerId_: number|null = null;
  private pageHandler_: PageHandlerRemote;
  private query_: string = '';
  private result_: QueryResult = new QueryResult();
  private title_: string = '';
  private visitsToBeRemoved_: Array<URLVisit> = [];

  //============================================================================
  // Overridden methods
  //============================================================================

  constructor() {
    super();
    this.pageHandler_ = BrowserProxy.getInstance().handler;
    this.callbackRouter_ = BrowserProxy.getInstance().callbackRouter;
  }

  connectedCallback() {
    super.connectedCallback();
    this.onClustersQueryResultListenerId_ =
        this.callbackRouter_.onClustersQueryResult.addListener(
            this.onClustersQueryResult_.bind(this));
    this.onVisitsRemovedListenerId_ =
        this.callbackRouter_.onVisitsRemoved.addListener(
            this.onVisitsRemoved_.bind(this));
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this.callbackRouter_.removeListener(
        assert(this.onClustersQueryResultListenerId_!));
    this.onClustersQueryResultListenerId_ = null;
    this.callbackRouter_.removeListener(
        assert(this.onVisitsRemovedListenerId_!));
    this.onVisitsRemovedListenerId_ = null;
  }

  //============================================================================
  // Event handlers
  //============================================================================

  private onCancelButtonClick_() {
    this.visitsToBeRemoved_ = [];
    this.$.confirmationDialog.get().close();
  }

  /**
   * Called with `event` received from a cluster that should be removed or
   * restructured due to all its visits or its top visit having been removed.
   * Contains the id of the Cluster in question.
   * @private
   */
  private onClusterChangedOrRemoved_(event: CustomEvent<bigint>) {
    // Request up to as many of the freshest clusters as currently shown until
    // now.
    this.onBrowserIdle_().then(() => {
      this.queryClusters_({
        query: this.query_.trim(),
        maxTime: undefined,
        maxCount: this.result_.clusters.length,
      });
    });
  }

  private onConfirmationDialogCancel_() {
    this.visitsToBeRemoved_ = [];
  }

  private onRemoveButtonClick_() {
    this.pageHandler_.removeVisits(this.visitsToBeRemoved_)
        .then(({accepted}) => {
          if (!accepted) {
            this.visitsToBeRemoved_ = [];
          }
        });
    this.$.confirmationDialog.get().close();
  }

  /**
   * Called with `event` received from a visit requesting to be removed. `event`
   * may contain the related visits of the said visit, if applicable.
   */
  private onRemoveVisits_(event: CustomEvent<Array<URLVisit>>) {
    // Return early if there is a pending remove request.
    if (this.visitsToBeRemoved_.length) {
      return;
    }

    this.visitsToBeRemoved_ = event.detail;
    this.$.confirmationDialog.get().showModal();
  }

  /**
   * Called when the value of the search field changes.
   */
  private onSearchChanged_(event: CustomEvent<string>) {
    // Update the query based on the value of the search field, if necessary.
    if (event.detail !== this.query_) {
      this.query_ = event.detail;
    }
  }

  /**
   * Called when the scrollable area has been scrolled nearly to the bottom.
   */
  private onScrolledToBottom_() {
    this.$.scrollThreshold.clearTriggers();

    if (this.result_ && this.result_.continuationMaxTime) {
      this.queryClusters_({
        query: this.result_.query,
        maxTime: this.result_.continuationMaxTime,
        maxCount: RESULTS_PER_PAGE,
      });
    }
  }

  //============================================================================
  // Helper methods
  //============================================================================

  private computeTitle_(): string {
    return this.result_ ?
        loadTimeData.getStringF('headerTitle', this.result_.query || '') :
        '';
  }

  private getSearchField_(): CrToolbarSearchFieldElement {
    return this.$.toolbar.getSearchField();
  }

  /**
   * Returns a promise that resolves when the browser is idle.
   */
  private onBrowserIdle_(): Promise<void> {
    return new Promise(resolve => {
      window.requestIdleCallback(() => {
        resolve();
      });
    });
  }

  private onClustersQueryResult_(result: QueryResult) {
    if (result.isContinuation) {
      // Do not replace the existing result. `result` contains a partial set of
      // Clusters that should be appended to the existing ones.
      this.push('result_.clusters', ...result.clusters);
      this.result_.continuationMaxTime = result.continuationMaxTime;
    } else {
      this.result_ = result;
    }
  }

  private onQueryChanged_() {
    // Update the value of the search field based on the query, if necessary.
    const searchField = this.getSearchField_();
    if (searchField.getValue() !== this.query_) {
      searchField.setValue(this.query_);
    }

    this.onBrowserIdle_().then(() => {
      // Request up to `RESULTS_PER_PAGE` of the freshest Clusters until now.
      this.queryClusters_({
        query: this.query_.trim(),
        maxTime: undefined,
        maxCount: RESULTS_PER_PAGE,
      });
      // Scroll to the top when the results change due to query change.
      this.$.container.scrollTop = 0;
    });
  }

  /**
   * Called when the last accepted request to browser to remove visits succeeds.
   */
  private onVisitsRemoved_() {
    this.visitsToBeRemoved_ = [];
  }

  private queryClusters_(queryParams: QueryParams) {
    // Invalidate the existing `continuationMaxTime`, if any, in order to
    // prevent sending additional requests while a request is in-flight. A new
    // `continuationMaxTime` will be supplied with the new set of results.
    if (this.result_) {
      this.result_.continuationMaxTime = undefined;
    }
    this.pageHandler_.queryClusters(queryParams);
  }
}

customElements.define(HistoryClustersAppElement.is, HistoryClustersAppElement);
