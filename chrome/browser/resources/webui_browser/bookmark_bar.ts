// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './bookmark_element.js';
import './icons.html.js';

import {getCss as getCrHiddenStyleCss} from '//resources/cr_elements/cr_hidden_style_lit.css.js';
import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './bookmark_bar.html.js';
import type {BookmarkData} from './bookmark_bar.mojom-webui.js';
import {BookmarkType} from './bookmark_bar.mojom-webui.js';
import type {BrowserProxy} from './bookmark_bar_browser_proxy.js';
import {BrowserProxyImpl} from './bookmark_bar_browser_proxy.js';

export class BookmarkBarElement extends CrLitElement {
  static get is() {
    return 'webui-browser-bookmark-bar';
  }

  static override get styles() {
    return getCrHiddenStyleCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      bookmarks_: {type: Array},
    };
  }

  protected accessor bookmarks_: BookmarkData[] = [];
  private listenerIds_: number[] = [];
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    const callbackRouter = this.browserProxy_.callbackRouter;
    this.listenerIds_ = [
      callbackRouter.show.addListener(this.show_.bind(this)),
      callbackRouter.hide.addListener(this.hide_.bind(this)),
      callbackRouter.bookmarkLoaded.addListener(
          this.loadBookmarkModel_.bind(this)),
      callbackRouter.favIconChanged.addListener(this.updateFavIcon_.bind(this)),
    ];

    this.loadBookmarkModel_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.listenerIds_.forEach(id => {
      assert(this.browserProxy_.callbackRouter.removeListener(id));
    });
    this.listenerIds_ = [];
  }

  private async loadBookmarkModel_() {
    this.bookmarks_ =
        (await this.browserProxy_.handler.getBookmarkBar()).bookmarks;
  }

  private updateFavIcon_(data: BookmarkData) {
    const index = this.bookmarks_.findIndex(item => item.id === data.id);
    if (index === -1) {
      return;
    }

    this.bookmarks_[index] = data;
    this.requestUpdate();
  }

  private show_() {
    this.hidden = false;
  }

  private hide_() {
    this.hidden = true;
  }

  protected onBookmarkClick_(e: Event) {
    e.preventDefault();

    const currentTarget = e.currentTarget as HTMLElement;
    const index = Number(currentTarget.dataset['index']);
    const bookmark = this.bookmarks_[index]!;

    // Only launch the bookmark if it's a URL.
    if (bookmark.type !== BookmarkType.URL) {
      return;
    }

    this.browserProxy_.handler.openInNewTab(bookmark.id);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'webui-browser-bookmark-bar': BookmarkBarElement;
  }
}

customElements.define(BookmarkBarElement.is, BookmarkBarElement);
