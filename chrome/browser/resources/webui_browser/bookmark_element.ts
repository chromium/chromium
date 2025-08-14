// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';

import {getFaviconForPageURL} from '//resources/js/icon.js';
import {CrLitElement, type PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import type {BookmarkData} from './bookmark_bar.mojom-webui.js';
import {BookmarkType} from './bookmark_bar.mojom-webui.js';
import {getCss} from './bookmark_element.css.js';
import {getHtml} from './bookmark_element.html.js';

export class BookmarkElement extends CrLitElement {
  static get is() {
    return 'webui-browser-bookmark-element';
  }

  static override get properties() {
    return {
      bookmarkTitle_: {type: String},
    };
  }

  bookmarkId: bigint;
  protected accessor bookmarkTitle_: string;
  protected bookmarkType_: BookmarkType;
  private static defaultFavIconUrl_: string = 'url(chrome://favicon2/)';
  private faviconUrl_: string = BookmarkElement.defaultFavIconUrl_;

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  constructor(data: BookmarkData) {
    super();
    this.bookmarkTitle_ = data.title;
    this.bookmarkId = data.id;
    this.bookmarkType_ = data.type;
    this.updateFavIcon(data.pageUrlForFavicon);
  }

  override update(changedProperties: PropertyValues) {
    if (this.bookmarkType_ !== BookmarkType.FOLDER) {
      this.style.setProperty('--favicon-url', `${this.faviconUrl_}`);
    }
    super.update(changedProperties);
  }

  updateFavIcon(pageUrlForFavicon: Url|null) {
    // getFaviconForPageURL, given a page URL, will construct a
    // chrome://favicon2/ URL that will request the icon from our local
    // cache considering sizes, resolution, etc.
    if (pageUrlForFavicon) {
      this.faviconUrl_ = getFaviconForPageURL(pageUrlForFavicon.url, false);
    } else {
      this.faviconUrl_ = BookmarkElement.defaultFavIconUrl_;
    }
  }

  protected handleClick_(e: MouseEvent) {
    e = e || window.event;
    e.preventDefault();

    // Only launch the bookmark if it's a URL.
    if (this.bookmarkType_ === BookmarkType.URL) {
      this.dispatchEvent(new CustomEvent('bookmark-click', {
        bubbles: true,
        composed: true,
        detail: {bookmarkId: this.bookmarkId},
      }));
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'webui-browser-bookmark-element': BookmarkElement;
  }
}

customElements.define(BookmarkElement.is, BookmarkElement);
