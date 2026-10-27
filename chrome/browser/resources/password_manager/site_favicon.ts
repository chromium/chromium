// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'site-favicon' is the section to display the favicon given the
 * |url| which can be used to download favicon. If downloading fails |origin|
 * will be used as a fallback to obtain the favicon from cache.
 */
import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';

import {getFavicon, getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './site_favicon.css.js';
import {getHtml} from './site_favicon.html.js';

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

export class SiteFaviconElement extends CrLitElement {
  static get is() {
    return 'site-favicon';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      domain: {type: String},
      url: {type: String},
      showDownloadedIcon_: {type: Boolean},
    };
  }

  accessor domain: string = '';
  accessor url: string = '';
  protected accessor showDownloadedIcon_: boolean = false;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('url')) {
      this.showDownloadedIcon_ = false;
    }
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);

    // Set a timeout to handle a case when image takes too long to load.
    setTimeout(() => {
      if (!this.$.downloadedFavicon.complete) {
        // Reset src to cancel ongoin request.
        this.$.downloadedFavicon.src = '';
      }
    }, 1000);
  }

  protected getBackgroundImage_() {
    if (this.domain) {
      const url = ensureUrlHasScheme(this.domain);
      return getFaviconForPageURL(url || '', false);
    }
    return getFavicon('');
  }

  protected onLoad_() {
    this.showDownloadedIcon_ = true;
    this.fire('site-favicon-loaded');
  }

  protected onLoadError_() {
    this.showDownloadedIcon_ = false;
    this.fire('site-favicon-error');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'site-favicon': SiteFaviconElement;
  }
}

customElements.define(SiteFaviconElement.is, SiteFaviconElement);
