// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './memory_card.js';
import './page_thumbnail.js';
import './router.js';
import './shared_vars.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';

import {MemoriesResult, PageCallbackRouter, PageHandlerRemote} from '/chrome/browser/ui/webui/memories/memories.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';
import {MojomConversionMixinBase} from './mojom_conversion_mixin.js';

/**
 * @fileoverview This file provides the root custom element for the Memories
 * landing page.
 */

/** @polymer */
class MemoriesAppElement extends MojomConversionMixinBase {
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
       * The currently displayed Memories returned by the browser in response to
       * a request for Memories related to a given query or those within a given
       * timespan.
       * @private {!MemoriesResult}
       */
      result_: Object,

      /**
       * The current query for which related Memories are requested and shown.
       * @private {string}
       */
      query_: {
        type: String,
        observer: 'onQueryChanged_',
      },
    };
  }

  constructor() {
    super();
    /** @private {PageHandlerRemote} */
    this.pageHandler_ = BrowserProxy.getInstance().handler;
    /** @private {!PageCallbackRouter} */
    this.callbackRouter_ = BrowserProxy.getInstance().callbackRouter;
  }

  //============================================================================
  // Event handlers
  //============================================================================

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

  /** @private */
  onQueryChanged_() {
    // Update the value of the search field based on the query, if necessary.
    const searchField = this.getSearchField_();
    if (searchField.getValue() !== this.query_) {
      searchField.setValue(this.query_);
    }
    // <if expr="not is_official_build">
    this.onBrowserIdle_().then(() => {
      this.pageHandler_.getSampleMemories(this.query_.trim())
          .then(({result}) => {
            this.result_ = result;
          });
    });
    // </if>
  }
}

customElements.define(MemoriesAppElement.is, MemoriesAppElement);
