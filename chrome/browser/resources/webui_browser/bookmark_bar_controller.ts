// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BookmarkBar} from './bookmark_bar.js';
import type {BookmarkData} from './bookmark_bar.mojom-webui.js';
import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './bookmark_bar.mojom-webui.js';

export class BookmarkBarController {
  // @ts-expect-error: initialized in init_().
  private bookmarkBar_: BookmarkBar;
  callbackRouter: PageCallbackRouter = new PageCallbackRouter();
  handler: PageHandlerRemote = new PageHandlerRemote();

  constructor() {
    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  init(bookmarkBar: BookmarkBar) {
    this.bookmarkBar_ = bookmarkBar;
    this.registerBookmarkChangeCallbacks_();
    this.loadBookmarkModel_();
  }

  launchBookmark(id: bigint) {
    this.handler.openInNewTab(id);
  }

  private registerBookmarkChangeCallbacks_() {
    this.callbackRouter.show.addListener(this.show_.bind(this));
    this.callbackRouter.hide.addListener(this.hide_.bind(this));
    this.callbackRouter.bookmarkLoaded.addListener(
        this.loadBookmarkModel_.bind(this));
    this.callbackRouter.favIconChanged.addListener(
        this.favIconChanged_.bind(this));
  }

  private async loadBookmarkModel_() {
    const {bookmarks} = await this.handler.getBookmarkBar();
    this.bookmarkBar_.resetBookmarks();
    bookmarks.forEach(bookmark => this.addBookmark_(bookmark));
  }

  private addBookmark_(data: BookmarkData) {
    this.bookmarkBar_.addBookmark(data);
  }

  private show_() {
    this.bookmarkBar_.show();
  }

  private hide_() {
    this.bookmarkBar_.hide();
  }

  private favIconChanged_(bookmarkData: BookmarkData) {
    this.bookmarkBar_.updateFavIcon(bookmarkData);
  }
}
