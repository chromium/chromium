// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './page_favicon.js';
import './shared_vars.js';
import './top_visit.js';

import {PageCallbackRouter} from '/chrome/browser/ui/webui/history_clusters/history_clusters.mojom-webui.js';
import {Cluster, URLVisit} from '/components/history_clusters/core/history_clusters.mojom-webui.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';
import {getHostnameFromUrl} from './utils.js';

/**
 * @fileoverview This file provides a custom element displaying a Cluster.
 */

class ClusterCardElement extends PolymerElement {
  static get is() {
    return 'history-cluster';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      //========================================================================
      // Public properties
      //========================================================================

      /**
       * The Cluster displayed by this element.
       * @type {!Cluster}
       */
      cluster: Object,
    };
  }

  constructor() {
    super();
    /** @private {!PageCallbackRouter} */
    this.callbackRouter_ = BrowserProxy.getInstance().callbackRouter;
    /** @private {?number} */
    this.onVisitsRemovedListenerId_ = null;
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.onVisitsRemovedListenerId_ =
        this.callbackRouter_.onVisitsRemoved.addListener(
            this.onVisitsRemoved_.bind(this));
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    this.callbackRouter_.removeListener(
        assert(this.onVisitsRemovedListenerId_));
    this.onVisitsRemovedListenerId_ = null;
  }

  //============================================================================
  // Helper methods
  //============================================================================

  /**
   * @param {!Array} array
   * @param {number} num
   * @return {!Array} Shallow copy of the first `num` items of the input array.
   * @private
   */
  arrayItems_(array, num) {
    return array.slice(0, num);
  }

  /**
   * @param {!Url} url
   * @return {string} The domain name of the URL without the leading 'www.'.
   * @private
   */
  getHostnameFromUrl_(url) {
    return getHostnameFromUrl(url);
  }

  /**
   * Called with the original remove params when the last accepted request to
   * browser to remove visits succeeds. Since the same visit may appear in
   * multiple Clusters, all clusters receive this callback in order to get a
   * chance to remove their matching visits.
   * @param {!Array<!URLVisit>} removedVisits
   * @private
   */
  onVisitsRemoved_(removedVisits) {
    // A matching visit is a visit to the removed visit's URL whose timespan
    // falls within that of the removed visit.
    const matchingVisit = (visit) => {
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

    // If no more visits are left in the Cluster, notify the enclosing
    // <clusters-app> to remove this Cluster element from the page.
    if (this.cluster.visits.length === 0) {
      this.dispatchEvent(new CustomEvent('remove-empty-cluster-element', {
        bubbles: true,
        composed: true,
        detail: this.cluster.id,
      }));
    }
  }
}

customElements.define(ClusterCardElement.is, ClusterCardElement);
