// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './page_favicon.js';
import './shared_vars.js';
import './top_visit.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';
import {Cluster, PageCallbackRouter, URLVisit} from './history_clusters.mojom-webui.js';

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
    const visitHasBeenRemoved = (visit: URLVisit) => {
      return removedVisits.findIndex((removedVisit) => {
        return visit.normalizedUrl.url === removedVisit.normalizedUrl.url &&
            visit.lastVisitTime.internalValue <=
            removedVisit.lastVisitTime.internalValue &&
            visit.firstVisitTime.internalValue >=
            removedVisit.firstVisitTime.internalValue;
      }) !== -1;
    };
    this.cluster.visits.forEach((topVisit: URLVisit, visitIndex: number) => {
      // Reconstitute each cluster by flattening the list of visits and related
      // visits, and removing the gone ones. Early exit if nothing removed.
      const allVisits = [topVisit, ...topVisit.relatedVisits];
      const remainingVisits = allVisits.filter(v => !visitHasBeenRemoved(v));
      if (allVisits.length === remainingVisits.length) {
        return;
      }

      if (!remainingVisits.length) {
        // If all visits are gone, remove this top visit entirely.
        this.splice('cluster.visits', visitIndex, 1);
      } else {
        // Splice in the re-constituted top visit.
        const newTopVisit = remainingVisits.shift()!;
        this.splice('cluster.visits', visitIndex, 1, newTopVisit);

        // This looks weird, but it just replaces all the existing
        // `newTopVisit.relatedVisits` with the `remainingVisits` using
        // Polymer's special array mutation methods, so the DOM gets updated.
        this.splice(
            `cluster.visits.${visitIndex}.relatedVisits`, 0,
            newTopVisit.relatedVisits.length, ...remainingVisits);
      }
    });

    this.dispatchEvent(new CustomEvent('iron-resize', {
      bubbles: true,
      composed: true,
    }));

    // Now if all top visits for this cluster have been removed, send an event
    // to also remove this cluster from the list.
    if (!this.cluster.visits.length) {
      this.dispatchEvent(new CustomEvent('cluster-emptied', {
        bubbles: true,
        composed: true,
        detail: this.cluster,
      }));
    }
  }
}

customElements.define(HistoryClusterElement.is, HistoryClusterElement);
