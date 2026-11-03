// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';

import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {getCss} from './page_favicon.css.js';

/**
 * @fileoverview This file provides a custom element displaying a page favicon.
 */

class PageFaviconElement extends CrLitElement {
  static get is() {
    return 'page-favicon';
  }

  static override get styles() {
    return getCss();
  }

  static override get properties() {
    return {
      /* The URL for which the favicon is shown. */
      url: {type: String},

      /**
       * Whether the favicon should use fallback to host. Used for the purpose
       * of fetching fallback favicons when there is no local exact match.
       */
      fallbackToHost: {type: Boolean},

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

  accessor url: Url|undefined;
  accessor fallbackToHost: boolean = true;
  accessor isKnownToSync: boolean = false;
  accessor size: number = 16;

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
                this.url, this.isKnownToSync, '',
                /* --favicon-size */ this.size,
                /* forceLightMode */ false,
                /* fallbackToHost */ this.fallbackToHost));
      }
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'page-favicon': PageFaviconElement;
  }
}

customElements.define(PageFaviconElement.is, PageFaviconElement);
