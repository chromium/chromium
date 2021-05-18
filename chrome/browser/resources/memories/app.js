// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './memory_card.js';
import './page_thumbnail.js';
import './router.js';
import './shared_vars.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/iron-scroll-threshold/iron-scroll-threshold.js';

import {MemoriesResult, PageCallbackRouter, PageHandlerRemote} from '/chrome/browser/ui/webui/memories/memories.mojom-webui.js';
import {Visit} from '/components/history_clusters/core/memories.mojom-webui.js';
import {CrToolbarElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import {CrToolbarSearchFieldElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {UnguessableToken} from 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';

/**
 * @fileoverview This file provides the root custom element for the Memories
 * landing page.
 */

/** @type {number} */
const RESULTS_PER_PAGE = 5;

class MemoriesAppElement extends PolymerElement {
  static get is() {
    return 'memories-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      //========================================================================
      // Private properties
      //========================================================================

      /**
       * The current query for which related Memories are requested and shown.
       * @private {string}
       */
      query_: {
        type: String,
        observer: 'onQueryChanged_',
      },

      /**
       * Contains 1) the Memories returned by the browser in response to a
       * request for the freshest Memories related to a given query until a
       * given time threshold and 2) the optional continuation query parameters
       * returned alongside the Memories to be used in the follow-up request to
       * load older Memories
       * @private {MemoriesResult}
       */
      result_: Object,

      /**
       * The list of visits to be removed. A non-empty array indicates a pending
       * remove request to the browser.
       * @private {!Array<!Visit>}
       */
      visitsToBeRemoved_: {
        type: Object,
        value: [],
      },
    };
  }

  constructor() {
    super();
    /** @private {PageHandlerRemote} */
    this.pageHandler_ = BrowserProxy.getInstance().handler;
    /** @private {!PageCallbackRouter} */
    this.callbackRouter_ = BrowserProxy.getInstance().callbackRouter;
    /** @private {?number} */
    this.onMemoriesQueryResultListenerId_ = null;
    /** @private {?number} */
    this.onVisitsRemovedListenerId_ = null;
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.onMemoriesQueryResultListenerId_ =
        this.callbackRouter_.onMemoriesQueryResult.addListener(
            this.onMemoriesQueryResult_.bind(this));
    this.onVisitsRemovedListenerId_ =
        this.callbackRouter_.onVisitsRemoved.addListener(
            this.onVisitsRemoved_.bind(this));
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    this.callbackRouter_.removeListener(
        assert(this.onMemoriesQueryResultListenerId_));
    this.onMemoriesQueryResultListenerId_ = null;
    this.callbackRouter_.removeListener(
        assert(this.onVisitsRemovedListenerId_));
    this.onVisitsRemovedListenerId_ = null;
  }

  //============================================================================
  // Event handlers
  //============================================================================

  /**
   * @private
   */
  onCancelButtonTap_() {
    this.visitsToBeRemoved_ = [];
    this.$.confirmationDialog.get().close();
  }

  /**
   * @private
   */
  onConfirmationDialogCancel_() {
    this.visitsToBeRemoved_ = [];
  }

  /**
   * @private
   */
  onRemoveButtonTap_() {
    this.pageHandler_.removeVisits(this.visitsToBeRemoved_)
        .then(({accepted}) => {
          if (!accepted) {
            this.visitsToBeRemoved_ = [];
          }
        });
    this.$.confirmationDialog.get().close();
  }

  /**
   * @param {CustomEvent<!UnguessableToken>} event Event received from an empty
   *     Memory whose visits have been removed entirely and it should also be
   *     removed from the page. Contains the id of the Memory to be removed.
   * @private
   */
  onRemoveEmptyMemoryEement_(event) {
    const index = this.result_.memories.findIndex((memory) => {
      return memory.id === event.detail;
    });
    if (index > -1) {
      this.splice('result_.memories', index, 1);
    }
  }

  /**
   * @param {CustomEvent<!Array<!Visit>>} event Event received from a visit
   *     requesting to be removed. The array may contain the related visits of
   *     the said visit, if applicable.
   * @private
   */
  onRemoveVisits_(event) {
    // Return early if there is a pending remove request.
    if (this.visitsToBeRemoved_.length) {
      return;
    }

    this.visitsToBeRemoved_ = event.detail;
    this.$.confirmationDialog.get().showModal();
  }

  /**
   * Called when the value of the search field changes.
   * @param {!CustomEvent<string>} e
   * @private
   */
  onSearchChanged_(e) {
    // Update the query based on the value of the search field, if necessary.
    if (e.detail !== this.query_) {
      this.query_ = e.detail;
    }
  }

  /**
   * Called when the scrollable area has been scrolled nearly to the bottom.
   * @private
   */
  onScrolledToBottom_() {
    /** @type {IronScrollThresholdElement} */ (this.$['scroll-threshold'])
        .clearTriggers();

    if (this.result_ && this.result_.continuationQueryParams) {
      this.pageHandler_.queryMemories(this.result_.continuationQueryParams);
      // Invalidate the existing continuation query params.
      this.result_.continuationQueryParams = null;
    }
  }

  //============================================================================
  // Helper methods
  //============================================================================

  /**
   * @param {Url} thumbnailUrl
   * @return {!{thumbnailUrl: Url}} WebPage with the thumbnailUrl property only.
   * @private
   */
  createPageWithThumbnail_(thumbnailUrl) {
    return {thumbnailUrl};
  }

  /**
   * @return {!CrToolbarSearchFieldElement}
   * @private
   */
  getSearchField_() {
    const crToolbarElement = /** @type {CrToolbarElement} */ (
        this.shadowRoot.querySelector('cr-toolbar'));
    return crToolbarElement.getSearchField();
  }

  /**
   * @return {!Promise} A promise that resolves when the browser is idle.
   * @private
   */
  onBrowserIdle_() {
    return new Promise((resolve) => {
      window.requestIdleCallback(resolve);
    });
  }

  /**
   * @private
   * @param {!MemoriesResult} result
   */
  onMemoriesQueryResult_(result) {
    if (result.isContinuation) {
      // Do not replace the existing result. `result` contains a partial set of
      // memories that should be appended to the existing ones.
      this.push('result_.memories', ...result.memories);
      this.result_.continuationQueryParams = result.continuationQueryParams;
    } else {
      this.$.container.scrollTop = 0;
      this.result_ = result;
    }
  }

  /** @private */
  onQueryChanged_() {
    // Update the value of the search field based on the query, if necessary.
    const searchField = this.getSearchField_();
    if (searchField.getValue() !== this.query_) {
      searchField.setValue(this.query_);
    }

    this.onBrowserIdle_().then(() => {
      // Request up to `RESULTS_PER_PAGE` of the freshest Memories until now.
      const queryParams = {
        query: this.query_.trim(),
        maxCount: RESULTS_PER_PAGE,
      };
      this.pageHandler_.queryMemories(queryParams);
      // Invalidate the existing continuation query params, if any.
      if (this.result_) {
        this.result_.continuationQueryParams = null;
      }
    });
  }

  /**
   * Called when the last accepted request to browser to remove visits succeeds.
   * @private
   */
  onVisitsRemoved_() {
    this.visitsToBeRemoved_ = [];
  }
}

customElements.define(MemoriesAppElement.is, MemoriesAppElement);
