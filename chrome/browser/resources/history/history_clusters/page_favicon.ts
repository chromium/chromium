// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview This file provides a custom element displaying a page favicon.
 */

declare global {
  interface HTMLElementTagNameMap {
    'page-favicon': PageFavicon,
  }
}

class PageFavicon extends PolymerElement {
  static get is() {
    return 'page-favicon';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The URL for which the favicon is shown.
       */
      url: Object,

      /**
       * The element's style attribute.
       */
      style: {
        type: String,
        reflectToAttribute: true,
        computed: `computeStyle_(url)`,
      },
    };
  }

  //============================================================================
  // Properties
  //============================================================================

  url: Url = new Url();

  //============================================================================
  // Helper methods
  //============================================================================

  private computeStyle_(): string {
    return `background-image:${
        getFaviconForPageURL(
            this.url.url, false, '', /** --favicon-size */ 16)}`;
  }
}

customElements.define(PageFavicon.is, PageFavicon);
