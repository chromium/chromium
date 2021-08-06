// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cluster.js';
import './shared_style.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/iron-scroll-threshold/iron-scroll-threshold.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {IronScrollThresholdElement} from 'chrome://resources/polymer/v3_0/iron-scroll-threshold/iron-scroll-threshold.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';
import {Cluster, PageCallbackRouter, PageHandlerRemote, QueryParams, QueryResult, URLVisit} from './history_clusters.mojom-webui.js';

/**
 * @fileoverview This file provides a custom element that requests and shows
 * history clusters given a query. It handles loading more clusters using
 * infinite scrolling as well as deletion of visits within the clusters.
 */

// Chosen fairly arbitrarily. We want to fill the whole vertical viewport.
// See `onClustersQueryResult_()` for details.
const RESULTS_PER_PAGE: number = 10;

declare global {
  interface HTMLElementTagNameMap {
    'history-clusters': HistoryClustersElement,
  }

  interface Window {
    // https://github.com/microsoft/TypeScript/issues/40807
    requestIdleCallback(callback: () => void): void;
  }
}

interface HistoryClustersElement {
  $: {
    confirmationDialog: CrLazyRenderElement<CrDialogElement>,
    confirmationToast: CrLazyRenderElement<CrToastElement>,
    container: Element,
    scrollThreshold: IronScrollThresholdElement,
  };
}

class HistoryClustersElement extends PolymerElement {
  static get is() {
    return 'history-clusters';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The current query for which related Clusters are requested and shown.
       */
      query: {
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

  query: string = '';
  private callbackRouter_: PageCallbackRouter;
  private onClustersQueryResultListenerId_: number|null = null;
  private onVisitsRemovedListenerId_: number|null = null;
  private pageHandler_: PageHandlerRemote;
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

  private onClusterEmptied_(event: CustomEvent<Cluster>) {
    // Find and remove the emptied cluster from the list. We don't pass an
    // index, as then that's one more piece of state to keep consistent.
    if (this.result_ && this.result_.clusters) {
      const index = this.result_.clusters.indexOf(event.detail);
      if (index !== -1) {
        this.splice('result_.clusters', index, 1);
      }
    }
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
    if (assert(this.visitsToBeRemoved_.length) > 1) {
      this.$.confirmationDialog.get().showModal();
    } else {
      // Bypass the confirmation dialog if removing one visit only.
      this.onRemoveButtonClick_();
    }
  }

  /**
   * Called when the value of the search field changes.
   */
  private onSearchChanged_(event: CustomEvent<string>) {
    // Update the query based on the value of the search field, if necessary.
    if (event.detail !== this.query) {
      this.query = event.detail;
    }
  }

  /**
   * Called when the scrollable area has been scrolled nearly to the bottom.
   */
  private onScrolledToBottom_() {
    this.$.scrollThreshold.clearTriggers();

    if (this.result_ && this.result_.continuationEndTime) {
      this.queryClusters_({
        query: this.result_.query,
        maxCount: RESULTS_PER_PAGE,
        endTime: this.result_.continuationEndTime,
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
      this.result_.continuationEndTime = result.continuationEndTime;
    } else {
      this.result_ = result;
    }

    // Handle the "tall monitor" edge case: if the returned results are are
    // shorter than the vertical viewport, the "container" div will not have a
    // scrollbar, and the user will never be able to trigger the
    // iron-scroll-threshold to request more results. Therefore, immediately
    // request more results if there is no scrollbar to fill the viewport.
    //
    // This should happen quite rarely in the queryless state since the backend
    // transparently tries to get at least ~100 visits to cluster.
    //
    // This is likely to happen very frequently in the search query state, since
    // many clusters will not match the search query and will be discarded.
    //
    // Do this on browser idle to avoid jank and to give the DOM a chance to be
    // updated with the results we just got.
    this.onBrowserIdle_().then(() => {
      const container = this.$.container;
      if (container.scrollHeight <= container.clientHeight) {
        this.onScrolledToBottom_();
      }
    });
  }

  private onQueryChanged_() {
    this.onBrowserIdle_().then(() => {
      // Request up to `RESULTS_PER_PAGE` of the freshest Clusters until now.
      this.queryClusters_({
        query: this.query.trim(),
        maxCount: RESULTS_PER_PAGE,
        endTime: undefined,
      });
      // Scroll to the top when the results change due to query change.
      this.$.container.scrollTop = 0;
    });
  }

  /**
   * Called when the last accepted request to browser to remove visits succeeds.
   */
  private onVisitsRemoved_() {
    // Show the confirmation toast once done removing one visit only; since a
    // confirmation dialog was not shown prior to the action.
    if (assert(this.visitsToBeRemoved_.length) === 1) {
      this.$.confirmationToast.get().show();
    }
    this.visitsToBeRemoved_ = [];
  }

  private queryClusters_(queryParams: QueryParams) {
    // Invalidate the existing `continuationEndTime`, if any, in order to
    // prevent sending additional requests while a request is in-flight. A new
    // `continuationEndTime` will be supplied with the new set of results.
    if (this.result_) {
      this.result_.continuationEndTime = undefined;
    }
    this.pageHandler_.queryClusters(queryParams);
  }
}

customElements.define(HistoryClustersElement.is, HistoryClustersElement);
