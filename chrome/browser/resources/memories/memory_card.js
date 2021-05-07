// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './memory_tile.js';
import './page_favicon.js';
import './page_thumbnail.js';
import './search_query.js';
import './shared_vars.js';
import './top_visit.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';

import {PageCallbackRouter, PageHandlerRemote} from '/chrome/browser/ui/webui/memories/memories.mojom-webui.js';
import {Memory, Visit} from '/components/history_clusters/core/memories.mojom-webui.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';
import {getHostnameFromUrl} from './utils.js';

/**
 * @fileoverview This file provides a custom element displaying a Memory.
 */

class MemoryCardElement extends PolymerElement {
  static get is() {
    return 'memory-card';
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
       * The Memory displayed by this element.
       * @type {!Memory}
       */
      memory: Object,

      //========================================================================
      // Private properties
      //========================================================================

      /**
       * Whether the Memory has related tab groups or bookmarks.
       * @private {boolean}
       */
      hasRelatedTabGroupsOrBookmarks_: {
        type: Boolean,
        computed: 'computeHasRelatedTabGroupsOrBookmarks_(memory)'
      },
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

  /** @private */
  computeHasRelatedTabGroupsOrBookmarks_() {
    return this.memory.relatedTabGroups.length > 0 ||
        this.memory.bookmarks.length > 0;
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
   * multiple Memories, all memories receive this callback in order to get a
   * chance to remove their matching visits.
   * @param {!Array<!Visit>} removedVisits
   * @private
   */
  onVisitsRemoved_(removedVisits) {
    // A matching visit is a visit to the removed visit's URL whose timespan
    // falls within that of the removed visit.
    const matchingVisit = (visit) => {
      return removedVisits.findIndex((removedVisit) => {
        return visit.url.url === removedVisit.url.url &&
            visit.time.internalValue <= removedVisit.time.internalValue &&
            visit.firstVisitTime.internalValue >=
            removedVisit.firstVisitTime.internalValue;
      }) !== -1;
    };
    this.memory.topVisits.forEach((topVisit, topVisitIndex) => {
      if (matchingVisit(topVisit)) {
        this.splice('memory.topVisits', topVisitIndex, 1);
        return;
      }
      topVisit.relatedVisits.forEach((relatedVisit, relatedVisitIndex) => {
        if (matchingVisit(relatedVisit)) {
          this.splice(
              `memory.topVisits.${topVisitIndex}.relatedVisits`,
              relatedVisitIndex, 1);
          return;
        }
      });
    });

    // If no more visits are left in the Memory, notify the enclosing
    // <memories-app> to remove this Memory element from the page.
    if (this.memory.topVisits.length === 0) {
      this.dispatchEvent(new CustomEvent('remove-empty-memory-element', {
        bubbles: true,
        composed: true,
        detail: this.memory.id,
      }));
    }
  }
}

customElements.define(MemoryCardElement.is, MemoryCardElement);
