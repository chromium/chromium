// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'site-favicon' is the section to display the favicon given the
 * site URL.
 */

import {getFavicon, getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './site_favicon.css.js';
import {getHtml} from './site_favicon.html.js';

export interface SiteFaviconElement {
  $: {
    favicon: HTMLElement,
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
      faviconUrl: {type: String},
      url: {type: String},
      // The icon's local path. We don't need to fetch the icon from the url if
      // the path is not empty.
      iconPath: {type: String},
    };
  }

  accessor faviconUrl: string = '';
  accessor url: string = '';
  accessor iconPath: string = '';

  protected getBackgroundImage_() {
    let backgroundImage = getFavicon('');
    if (this.iconPath) {
      backgroundImage = 'url(' + this.iconPath + ')';
    } else if (this.faviconUrl) {
      const url = this.ensureUrlHasScheme_(this.faviconUrl);
      backgroundImage = getFavicon(url);
    } else if (this.url) {
      let url = this.removePatternWildcard_(this.url);
      url = this.ensureUrlHasScheme_(url);
      backgroundImage = getFaviconForPageURL(url || '', false);
    }
    return backgroundImage;
  }

  /**
   * Removes the wildcard prefix from a pattern string.
   * @param pattern The pattern to remove the wildcard from.
   * @return The resulting pattern.
   */
  private removePatternWildcard_(pattern: string): string {
    if (!pattern || pattern.length === 0) {
      return pattern;
    }

    if (pattern.startsWith('http://[*.]')) {
      return pattern.replace('http://[*.]', 'http://');
    } else if (pattern.startsWith('https://[*.]')) {
      return pattern.replace('https://[*.]', 'https://');
    } else if (pattern.startsWith('[*.]')) {
      return pattern.substring(4, pattern.length);
    }
    return pattern;
  }

  /**
   * Ensures the URL has a scheme (assumes http if omitted).
   * @param url The URL with or without a scheme.
   * @return The URL with a scheme, or an empty string.
   */
  private ensureUrlHasScheme_(url: string): string {
    if (!url || url.length === 0) {
      return url;
    }
    return url.includes('://') ? url : 'http://' + url;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'site-favicon': SiteFaviconElement;
  }
}

customElements.define(SiteFaviconElement.is, SiteFaviconElement);
