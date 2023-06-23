// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'site-favicon' is the section to display the favicon given the
 * |url| which can be used to download favicon. If downloading fails |origin|
 * will be used as a fallback to obtain the favicon from cache.
 */
import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';

import {getFavicon, getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './site_favicon.html.js';

export interface SiteFaviconElement {
  $: {
    favicon: HTMLElement,
    downloadedFavicon: HTMLImageElement,
  };
}

/**
 * Ensures the URL has a scheme (assumes http if omitted).
 * @param url The URL with or without a scheme.
 * @return The URL with a scheme, or an empty string.
 */
function ensureUrlHasScheme(url: string): string {
  return url.includes('://') ? url : 'http://' + url;
}

export class SiteFaviconElement extends PolymerElement {
  static get is() {
    return 'site-favicon';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      domain: String,
      url: {
        type: String,
        observer: 'onUrlChanged_',
      },

      showDownloadedIcon_: {
        type: Boolean,
        value: false,
      },
    };
  }

  domain: string;
  url: string;
  private showDownloadedIcon_: boolean;

  override ready() {
    super.ready();
    // Set a timeout to handle a case when image takes too long to load.
    setTimeout(() => {
      if (!this.$.downloadedFavicon.complete) {
        // Reset src to cancel ongoin request.
        this.$.downloadedFavicon.src = '';
      }
    }, 1000);
  }

  private getBackgroundImage_() {
    if (this.domain) {
      const url = ensureUrlHasScheme(this.domain);
      return getFaviconForPageURL(url || '', false);
    }
    return getFavicon('');
  }

  private onLoadSuccess_() {
    this.showDownloadedIcon_ = true;
    this.dispatchEvent(new CustomEvent(
        'site-favicon-loaded', {bubbles: true, composed: true}));
  }

  private onLoadError_() {
    this.showDownloadedIcon_ = false;
    this.dispatchEvent(
        new CustomEvent('site-favicon-error', {bubbles: true, composed: true}));
  }

  private onUrlChanged_() {
    this.showDownloadedIcon_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'site-favicon': SiteFaviconElement;
  }
}

customElements.define(SiteFaviconElement.is, SiteFaviconElement);
