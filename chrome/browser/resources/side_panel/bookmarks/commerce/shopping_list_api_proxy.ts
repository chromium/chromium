// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BookmarkProductInfo, PageCallbackRouter, ShoppingListHandlerFactory, ShoppingListHandlerRemote} from '../shopping_list.mojom-webui.js';

let instance: ShoppingListApiProxy|null = null;

export interface ShoppingListApiProxy {
  getAllPriceTrackedBookmarkProductInfo():
      Promise<{productInfos: BookmarkProductInfo[]}>;
  getAllShoppingBookmarkProductInfo():
      Promise<{productInfos: BookmarkProductInfo[]}>;
  trackPriceForBookmark(bookmarkId: bigint): void;
  untrackPriceForBookmark(bookmarkId: bigint): void;
  getCallbackRouter(): PageCallbackRouter;
}

export class ShoppingListApiProxyImpl implements ShoppingListApiProxy {
  handler: ShoppingListHandlerRemote;
  callbackRouter: PageCallbackRouter;

  constructor() {
    this.callbackRouter = new PageCallbackRouter();

    this.handler = new ShoppingListHandlerRemote();

    const factory = ShoppingListHandlerFactory.getRemote();
    factory.createShoppingListHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  getAllPriceTrackedBookmarkProductInfo() {
    return this.handler.getAllPriceTrackedBookmarkProductInfo();
  }

  getAllShoppingBookmarkProductInfo() {
    return this.handler.getAllShoppingBookmarkProductInfo();
  }

  trackPriceForBookmark(bookmarkId: bigint) {
    this.handler.trackPriceForBookmark(bookmarkId);
  }

  untrackPriceForBookmark(bookmarkId: bigint) {
    this.handler.untrackPriceForBookmark(bookmarkId);
  }

  getCallbackRouter() {
    return this.callbackRouter;
  }

  static getInstance(): ShoppingListApiProxy {
    return instance || (instance = new ShoppingListApiProxyImpl());
  }

  static setInstance(obj: ShoppingListApiProxy) {
    instance = obj;
  }
}
