// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_auto_img/cr_auto_img.js';

import {getFaviconForPageURL} from '//resources/js/icon.js';
import {CrLitElement, html} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import {getCss} from './page_favicon.css.js';

/**
 * @fileoverview This file provides a custom element displaying a page favicon.
 */

declare global {
  interface HTMLElementTagNameMap {
    'page-favicon': PageFavicon;
  }
}

class PageFavicon extends CrLitElement {
  static get is() {
    return 'page-favicon';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return html``;
  }

  static override get properties() {
    return {
      /* The URL for which the favicon is shown. */
      url: {type: Object},

      /**
       * Whether this visit is known to sync already. Used for the purpose of
       * fetching higher quality favicons in that case.
       */
      isKnownToSync: {type: Boolean},

      size: {type: Number},
    };
  }

  //============================================================================
  // Properties
  //============================================================================

  url?: Url;
  isKnownToSync: boolean = false;
  size: number = 16;

  //============================================================================
  // Helper methods
  //============================================================================

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('url')) {
      if (!this.url) {
        // Pages with a pre-set image URL or no favicon URL don't show the
        // favicon.
        this.style.setProperty('background-image', '');
      } else {
        this.style.setProperty(
            'background-image',
            getFaviconForPageURL(
                this.url.url, this.isKnownToSync, '',
                /* --favicon-size */ this.size));
      }
    }
  }
}

customElements.define(PageFavicon.is, PageFavicon);
