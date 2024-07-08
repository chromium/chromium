// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_auto_img/cr_auto_img.js';

import {getFavicon, getFaviconForPageURL} from '//resources/js/icon.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './site_favicon.css.js';
import {getHtml} from './site_favicon.html.js';

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

  protected domain: string = '';
  protected url: string = '';
  protected showDownloadedIcon_: boolean = false;

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('url')) {
      this.showDownloadedIcon_ = false;
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
    this.dispatchEvent(new CustomEvent(
        'site-favicon-loaded', {bubbles: true, composed: true}));
  }

  protected onLoadError_() {
    this.showDownloadedIcon_ = false;
    this.dispatchEvent(
        new CustomEvent('site-favicon-error', {bubbles: true, composed: true}));
  }

  protected onUrlChanged_() {
    this.showDownloadedIcon_ = false;
  }

  setDomainForTesting(domain: string) {
    this.domain = domain;
  }

  setUrlForTesting(url: string) {
    this.url = url;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'site-favicon': SiteFaviconElement;
  }
}

customElements.define(SiteFaviconElement.is, SiteFaviconElement);
