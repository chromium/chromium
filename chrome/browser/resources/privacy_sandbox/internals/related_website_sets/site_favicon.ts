// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_auto_img/cr_auto_img.js';

import {getFavicon, getFaviconForPageURL} from '//resources/js/icon.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './site_favicon.css.js';
import {getHtml} from './site_favicon.html.js';

const FAVICON_TIMEOUT_MS = 1000;

/**
 * Ensures the URL has a scheme (assumes http if omitted).
 * @param url The URL with or without a scheme.
 * @return The URL with a scheme, or an empty string.
 */
function ensureUrlHasScheme(url: string): string {
  return url.includes('://') ? url : 'https://' + url;
}

export interface SiteFaviconElement {
  $: {
    favicon: HTMLElement,
    downloadedFavicon: HTMLImageElement,
  };
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

  domain: string = '';
  url: string = '';
  protected showDownloadedIcon_: boolean = false;
  private faviconDownloadTimeout_: number|null = null;

  override disconnectedCallback() {
    super.disconnectedCallback();

    if (this.faviconDownloadTimeout_ !== null) {
      clearTimeout(this.faviconDownloadTimeout_);
      this.faviconDownloadTimeout_ = null;
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('url')) {
      this.onUrlChanged_();
    }
  }

  protected getBackgroundImage_() {
    if (!this.domain) {
      return getFavicon('');
    }
    const url = ensureUrlHasScheme(this.domain);
    return getFaviconForPageURL(url, false);
  }

  protected onLoadSuccess_() {
    this.showDownloadedIcon_ = true;
    this.faviconDownloadTimeout_ && clearTimeout(this.faviconDownloadTimeout_);
    this.faviconDownloadTimeout_ = null;
    this.dispatchEvent(new CustomEvent(
        'site-favicon-loaded', {bubbles: true, composed: true}));
  }

  protected onLoadError_() {
    this.showDownloadedIcon_ = false;
    this.dispatchEvent(
        new CustomEvent('site-favicon-error', {bubbles: true, composed: true}));
  }

  protected onUrlChanged_() {
    this.faviconDownloadTimeout_ = setTimeout(() => {
      if (!this.$.downloadedFavicon.complete) {
        // Reset src to cancel ongoing request.
        this.$.downloadedFavicon.src = '';
      }
    }, FAVICON_TIMEOUT_MS);
    this.showDownloadedIcon_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'site-favicon': SiteFaviconElement;
  }
}

customElements.define(SiteFaviconElement.is, SiteFaviconElement);
