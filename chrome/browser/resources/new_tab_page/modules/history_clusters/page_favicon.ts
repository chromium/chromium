// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';

import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './page_favicon.html.js';

/**
 * @fileoverview This file provides a custom element displaying a page favicon.
 */

declare global {
  interface HTMLElementTagNameMap {
    'page-favicon': PageFavicon;
  }
}

class PageFavicon extends PolymerElement {
  static get is() {
    return 'page-favicon';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /* The element's style attribute. */
      style: {
        type: String,
        computed: `computeStyle_(url, imageUrl)`,
        reflectToAttribute: true,
      },

      /* The URL for which the favicon is shown. */
      url: Object,

      /**
       * Whether this visit is known to sync already. Used for the purpose of
       * fetching higher quality favicons in that case.
       */
      isKnownToSync: Boolean,

      size: {
        type: Number,
        value: 16,
      },
    };
  }

  //============================================================================
  // Properties
  //============================================================================

  url: Url;
  isKnownToSync: boolean;
  size: number;

  //============================================================================
  // Helper methods
  //============================================================================

  private computeStyle_(): string {
    if (!this.url) {
      return '';
    }
    return `background-image:${
        getFaviconForPageURL(
            this.url.url, this.isKnownToSync, '',
            /** --favicon-size */ this.size)}`;
  }
}

customElements.define(PageFavicon.is, PageFavicon);
