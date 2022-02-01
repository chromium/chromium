// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './page_favicon.js';
import './shared_vars.js';
import './top_visit.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {getTemplate} from './cluster.html.js';
import {Cluster, PageCallbackRouter, URLVisit} from './history_clusters.mojom-webui.js';
import {ClusterAction, MetricsProxyImpl} from './metrics_proxy.js';

/**
 * @fileoverview This file provides a custom element displaying a cluster.
 */

declare global {
  interface HTMLElementTagNameMap {
    'history-cluster': HistoryClusterElement,
  }
}

class HistoryClusterElement extends PolymerElement {
  static get is() {
    return 'history-cluster';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The index of the cluster.
       */
      index: {
        type: Number,
        value: -1,  // Initialized to an invalid value.
      },

      /**
       * The cluster displayed by this element.
       */
      cluster: Object,
    };
  }

  //============================================================================
  // Properties
  //============================================================================

  cluster: Cluster;
  index: number;
  private callbackRouter_: PageCallbackRouter;
  private onVisitsRemovedListenerId_: number|null = null;

  //============================================================================
  // Overridden methods
  //============================================================================

  constructor() {
    super();
    this.callbackRouter_ = BrowserProxyImpl.getInstance().callbackRouter;

    // This element receives a tabindex, because it's an iron-list item.
    // However, what we really want to do is to pass that focus onto an
    // eligible child, so we set `delegatesFocus` to true.
    this.attachShadow({mode: 'open', delegatesFocus: true});
  }

  connectedCallback() {
    super.connectedCallback();
    this.onVisitsRemovedListenerId_ =
        this.callbackRouter_.onVisitsRemoved.addListener(
            this.onVisitsRemoved_.bind(this));
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.onVisitsRemovedListenerId_);
    this.callbackRouter_.removeListener(this.onVisitsRemovedListenerId_);
    this.onVisitsRemovedListenerId_ = null;
  }

  //============================================================================
  // Event handlers
  //============================================================================

  private onRelatedSearchClicked_() {
    MetricsProxyImpl.getInstance().recordClusterAction(
        ClusterAction.RELATED_SEARCH_CLICKED, this.index);
  }

  private onRelatedVisitsVisibilityToggled_() {
    MetricsProxyImpl.getInstance().recordClusterAction(
        ClusterAction.RELATED_VISITS_VISIBILITY_TOGGLED, this.index);
  }

  private onVisitClicked_() {
    MetricsProxyImpl.getInstance().recordClusterAction(
        ClusterAction.VISIT_CLICKED, this.index);
  }

  //============================================================================
  // Helper methods
  //============================================================================

  /**
   * Called with the original remove params when the last accepted request to
   * browser to remove visits succeeds. Since the same visit may appear in
   * multiple Clusters, all Clusters receive this callback in order to get a
   * chance to remove their matching visits.
   */
  private onVisitsRemoved_(removedVisits: Array<URLVisit>) {
    const visitHasBeenRemoved = (visit: URLVisit) => {
      return removedVisits.findIndex((removedVisit) => {
        return visit.normalizedUrl.url === removedVisit.normalizedUrl.url &&
            visit.lastVisitTime.internalValue <=
            removedVisit.lastVisitTime.internalValue &&
            visit.firstVisitTime.internalValue >=
            removedVisit.firstVisitTime.internalValue;
      }) !== -1;
    };

    // Flatten the cluster's constitutent visits and filter out the removed
    // ones, if any.
    const allVisits = [this.cluster.visit, ...this.cluster.visit.relatedVisits];
    const remainingVisits = allVisits.filter(v => !visitHasBeenRemoved(v));
    if (allVisits.length === remainingVisits.length) {
      return;
    }

    if (!remainingVisits.length) {
      // If all the visits are removed, fire an event to also remove this
      // cluster from the list of clusters.
      this.dispatchEvent(new CustomEvent('remove-cluster', {
        bubbles: true,
        composed: true,
        detail: this.index,
      }));
    } else {
      // Reconstitute the cluster by setting the top visit with the
      // `remainingVisits` as its related visits.
      this.set('cluster.visit', remainingVisits.shift()!);
      this.set('cluster.visit.relatedVisits', remainingVisits);
    }

    this.dispatchEvent(new CustomEvent('iron-resize', {
      bubbles: true,
      composed: true,
    }));
  }
}

customElements.define(HistoryClusterElement.is, HistoryClusterElement);
