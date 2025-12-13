// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';

import {getFaviconForPageURL} from '//resources/js/icon.js';
import {CrLitElement, type PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import type {BookmarkData} from './bookmark_bar.mojom-webui.js';
import {BookmarkType} from './bookmark_bar.mojom-webui.js';
import {getCss} from './bookmark_element.css.js';
import {getHtml} from './bookmark_element.html.js';

const DEFAULT_FAVICON_URL: string = 'url(chrome://favicon2/)';

export class BookmarkElement extends CrLitElement {
  static get is() {
    return 'webui-browser-bookmark-element';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      data: {type: Object},
    };
  }

  accessor data: BookmarkData = {
    id: 0n,
    title: '',
    type: BookmarkType.URL,
    pageUrlForFavicon: null,
  };

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (this.data.type !== BookmarkType.FOLDER) {
      this.style.setProperty('--favicon-url', `${this.computeFaviconUrl_()}`);
    }
  }

  private computeFaviconUrl_(): string {
    if (!this.data.pageUrlForFavicon) {
      return DEFAULT_FAVICON_URL;
    }

    // getFaviconForPageURL, given a page URL, will construct a
    // chrome://favicon2/ URL that will request the icon from our local
    // cache considering sizes, resolution, etc.
    return getFaviconForPageURL(this.data.pageUrlForFavicon.url, false);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'webui-browser-bookmark-element': BookmarkElement;
  }
}

customElements.define(BookmarkElement.is, BookmarkElement);
