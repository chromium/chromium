// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cluster.js';
import './shared_style.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/polymer/v3_0/iron-scroll-threshold/iron-scroll-threshold.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {FocusOutlineManager} from 'chrome://resources/js/cr/ui/focus_outline_manager.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {Time} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {IronScrollThresholdElement} from 'chrome://resources/polymer/v3_0/iron-scroll-threshold/iron-scroll-threshold.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {Cluster, PageCallbackRouter, PageHandlerRemote, QueryParams, QueryResult, URLVisit} from './history_clusters.mojom-webui.js';
import {ClusterAction, MetricsProxyImpl} from './metrics_proxy.js';

/**
 * @fileoverview This file provides a custom element that requests and shows
 * history clusters given a query. It handles loading more clusters using
 * infinite scrolling as well as deletion of visits within the clusters.
 */

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
    clusters: IronListElement,
    confirmationDialog: CrLazyRenderElement<CrDialogElement>,
    confirmationToast: CrLazyRenderElement<CrToastElement>,
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
       * The current query for which related clusters are requested and shown.
       */
      query: {
        type: String,
        observer: 'onQueryChanged_',
        value: '',
      },

      /**
       * The header text to show when the query and the results are non-empty.
       */
      headerText_: {
        type: String,
        computed: `computeHeaderText_(result_.*)`,
      },

      /**
       * The placeholder text to show when the results are empty.
       */
      placeholderText_: {
        type: String,
        computed: `computePlaceholderText_(result_.*)`,
      },

      /**
       * The browser response to a request for the freshest clusters related to
       * a given query until an optional given end time (or the present time).
       * Contains the clusters, the optional continuation end time to be used in
       * the follow-up request to load older clusters, and the original query.
       */
      result_: Object,

      /**
       * The list of visits to be removed. A non-empty array indicates a pending
       * remove request to the browser.
       */
      visitsToBeRemoved_: {
        type: Object,
        value: () => [],
      },
    };
  }

  //============================================================================
  // Properties
  //============================================================================

  query: string;
  private callbackRouter_: PageCallbackRouter;
  private onClustersQueryResultListenerId_: number|null = null;
  private onVisitsRemovedListenerId_: number|null = null;
  private pageHandler_: PageHandlerRemote;
  private result_: QueryResult;
  private visitsToBeRemoved_: Array<URLVisit>;

  //============================================================================
  // Overridden methods
  //============================================================================

  constructor() {
    super();
    this.pageHandler_ = BrowserProxyImpl.getInstance().handler;
    this.callbackRouter_ = BrowserProxyImpl.getInstance().callbackRouter;
  }

  connectedCallback() {
    super.connectedCallback();

    // Register a per-document singleton focus outline manager. Some of our
    // child elements depend on the CSS classes set by this singleton.
    FocusOutlineManager.forDocument(document);

    this.$.clusters.notifyResize();
    this.$.clusters.scrollTarget = this;
    this.$.scrollThreshold.scrollTarget = this;

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

  private onConfirmationDialogCancel_() {
    this.visitsToBeRemoved_ = [];
  }

  private onLoadMoreButtonClick_() {
    if (this.result_ && this.result_.continuationEndTime) {
      this.queryClusters_({
        query: this.result_.query,
        endTime: this.result_.continuationEndTime,
      });
    }
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
   * Called with `event` received from a cluster requesting to be removed from
   * the list when all its visits have been removed. Contains the cluster index.
   */
  private onRemoveCluster_(event: CustomEvent<number>) {
    const index = event.detail;
    this.splice('result_.clusters', index, 1);
    MetricsProxyImpl.getInstance().recordClusterAction(
        ClusterAction.DELETED, index);
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

    if (this.shadowRoot!.querySelector(':focus-visible')) {
      // If some element of ours is keyboard-focused, don't automatically load
      // more clusters. It loses the user's position and messes up screen
      // readers. Let the user manually click the "Load More" button, if needed.
      // We use :focus-visible here, because :focus is triggered by mouse focus
      // too. And `FocusOutlineManager.visible()` is too primitive. It's true
      // on page load, and whenever the user is typing in the searchbox.
      return;
    }

    this.onLoadMoreButtonClick_();
  }

  //============================================================================
  // Helper methods
  //============================================================================

  private computeHeaderText_(): string {
    return this.result_ && this.result_.query && this.result_.clusters.length ?
        loadTimeData.getStringF('headerText', this.result_.query) :
        '';
  }

  private computePlaceholderText_(): string {
    if (!this.result_) {
      return '';
    }
    return this.result_.clusters.length ?
        '' :
        loadTimeData.getString(
            this.result_.query ? 'noSearchResults' : 'noResults');
  }

  /**
   * Returns true and hides the button unless we actually have more results to
   * load. Note we don't actually hide this button based on keyboard-focus
   * state. This is because if the user is using the mouse, more clusters are
   * loaded before the user ever gets a chance to see this button.
   */
  private getLoadMoreButtonHidden_(
      _result: QueryResult, _result_clusters: Array<Cluster>,
      _result_continuation_time: Time): boolean {
    return !this.result_ || this.result_.clusters.length === 0 ||
        !this.result_.continuationEndTime;
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
      // Do not replace the existing result when `result` contains a partial
      // set of clusters that should be appended to the existing ones.
      this.push('result_.clusters', ...result.clusters);
      this.set('result_.continuationEndTime', result.continuationEndTime);
    } else {
      // Scroll to the top when `result` contains a new set of clusters.
      this.scrollTop = 0;
      this.result_ = result;
    }

    // Handle the "tall monitor" edge case: if the returned results are are
    // shorter than the vertical viewport, the <history-clusters> element will
    // not have a scrollbar, and the user will never be able to trigger the
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
      if (this.scrollHeight <= this.clientHeight) {
        this.onLoadMoreButtonClick_();
      }
    });
  }

  private onQueryChanged_() {
    this.onBrowserIdle_().then(() => {
      this.queryClusters_({
        query: this.query.trim(),
        endTime: undefined,
      });
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
