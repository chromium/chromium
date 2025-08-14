// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './bookmark_element.js';
import './icons.html.js';

import {CrLitElement, html} from '//resources/lit/v3_0/lit.rollup.js';

import type {BookmarkData} from './bookmark_bar.mojom-webui.js';
import {BookmarkElement} from './bookmark_element.js';

export class BookmarkBar extends CrLitElement {
  static get is() {
    return 'webui-browser-bookmark-bar';
  }

  static override get properties() {
    return {
      bookmarks_: {type: Array},
    };
  }

  protected accessor bookmarks_: BookmarkElement[] = [];

  override render() {
    return html`${this.bookmarks_}`;
  }

  addBookmark(data: BookmarkData) {
    const bookmarkElement = new BookmarkElement(data);
    this.bookmarks_ = [...this.bookmarks_, bookmarkElement];

    this.requestUpdate();
  }

  updateFavIcon(data: BookmarkData) {
    const updatedBookmark =
        this.bookmarks_.find(bookmark => bookmark.bookmarkId === data.id);
    if (updatedBookmark) {
      updatedBookmark.updateFavIcon(data.pageUrlForFavicon);
      this.requestUpdate();
    }
  }

  resetBookmarks() {
    // Reset the number of bookmarks to 0;
    this.bookmarks_ = [];
  }

  show() {
    this.dispatchEvent(new CustomEvent('show-bookmark-bar'));
  }

  hide() {
    this.dispatchEvent(new CustomEvent('hide-bookmark-bar'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'webui-browser-bookmark-bar': BookmarkBar;
  }
}

customElements.define(BookmarkBar.is, BookmarkBar);
