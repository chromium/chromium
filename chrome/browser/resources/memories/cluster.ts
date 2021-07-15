// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './page_favicon.js';
import './shared_vars.js';
import './top_visit.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';
import {PageCallbackRouter} from './chrome/browser/ui/webui/history_clusters/history_clusters.mojom-webui.js';
import {Cluster, URLVisit} from './components/history_clusters/core/history_clusters.mojom-webui.js';

/**
 * @fileoverview This file provides a custom element displaying a Cluster.
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
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The Cluster displayed by this element.
       */
      cluster: Object,
    };
  }

  //============================================================================
  // Properties
  //============================================================================

  cluster: Cluster = new Cluster();
  private callbackRouter_: PageCallbackRouter;
  private onVisitsRemovedListenerId_: number|null = null;

  //============================================================================
  // Overridden methods
  //============================================================================

  constructor() {
    super();
    this.callbackRouter_ = BrowserProxy.getInstance().callbackRouter;
  }

  connectedCallback() {
    super.connectedCallback();
    this.onVisitsRemovedListenerId_ =
        this.callbackRouter_.onVisitsRemoved.addListener(
            this.onVisitsRemoved_.bind(this));
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this.callbackRouter_.removeListener(
        assert(this.onVisitsRemovedListenerId_!));
    this.onVisitsRemovedListenerId_ = null;
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
    // A matching visit is a visit to the removed visit's URL whose timespan
    // falls within that of the removed visit.
    const matchingVisit = (visit: URLVisit) => {
      return removedVisits.findIndex((removedVisit) => {
        return visit.normalizedUrl.url === removedVisit.normalizedUrl.url &&
            visit.lastVisitTime.internalValue <=
            removedVisit.lastVisitTime.internalValue &&
            visit.firstVisitTime.internalValue >=
            removedVisit.firstVisitTime.internalValue;
      }) !== -1;
    };
    this.cluster.visits.forEach((visit, visitIndex) => {
      if (matchingVisit(visit)) {
        this.splice('cluster.visits', visitIndex, 1);
        return;
      }
      visit.relatedVisits.forEach((relatedVisit, relatedVisitIndex) => {
        if (matchingVisit(relatedVisit)) {
          this.splice(
              `cluster.visits.${visitIndex}.relatedVisits`, relatedVisitIndex,
              1);
          return;
        }
      });
    });

    // Depending on the selected option, removing a top visit results in the
    // Cluster to either be removed or restructured. Notify the enclosing
    // <clusters-app> to refresh the list of displayed Clusters.
    if (this.cluster.visits.length === 0) {
      this.dispatchEvent(new CustomEvent('cluster-changed-or-removed', {
        bubbles: true,
        composed: true,
        detail: this.cluster.id,
      }));
    }
  }
}

customElements.define(HistoryClusterElement.is, HistoryClusterElement);
