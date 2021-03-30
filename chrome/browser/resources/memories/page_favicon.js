// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getFaviconForPageURL} from 'chrome://resources/js/icon.m.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview This file provides a custom element displaying a page favicon.
 */

class PageFavicon extends PolymerElement {
  static get is() {
    return 'page-favicon';
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
       * The URL for which the favicon is shown.
       * @type {!Url}
       */
      url: Object,

      /**
       * The element's style attribute.
       * @type {string}
       */
      style: {
        type: String,
        reflectToAttribute: true,
        computed: `computeStyle_(url)`,
      },
    };
  }

  //============================================================================
  // Helper methods
  //============================================================================

  /** @private */
  computeStyle_() {
    return `background-image:${
        getFaviconForPageURL(this.url.url, false, '', 24)}`;
  }
}

customElements.define(PageFavicon.is, PageFavicon);
