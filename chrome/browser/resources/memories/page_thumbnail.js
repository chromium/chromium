// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shared_vars.js';

import {WebPage} from '/components/history_clusters/core/memories.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview This file provides a custom element displaying a page
 * thumbnail.
 */

class PageThumbnailElement extends PolymerElement {
  static get is() {
    return 'page-thumbnail';
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
       * The page for which the thumbnail is shown.
       * @type {!WebPage}
       */
      page: Object,
    };
  }

  //============================================================================
  // Helper methods
  //============================================================================

  /**
   * @param {Url} thumbnailUrl
   * @return {string}
   * @private
   */
  getThumbnailSrc_(thumbnailUrl) {
    return thumbnailUrl ? `chrome://image?${thumbnailUrl.url}` : '';
  }
}

customElements.define(PageThumbnailElement.is, PageThumbnailElement);
