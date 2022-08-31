// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BookmarkProductInfo, ShoppingListHandlerFactory, ShoppingListHandlerRemote} from './shopping_list.mojom-webui.js';

let instance: ShoppingListApiProxy|null = null;

export interface ShoppingListApiProxy {
  getAllPriceTrackedBookmarkProductInfo():
      Promise<{productInfos: BookmarkProductInfo[]}>;
  trackPriceForBookmark(bookmarkId: bigint): void;
  untrackPriceForBookmark(bookmarkId: bigint): void;
}

export class ShoppingListApiProxyImpl implements ShoppingListApiProxy {
  handler: ShoppingListHandlerRemote;

  constructor() {
    this.handler = new ShoppingListHandlerRemote();

    const factory = ShoppingListHandlerFactory.getRemote();
    factory.createShoppingListHandler(
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  getAllPriceTrackedBookmarkProductInfo() {
    return this.handler.getAllPriceTrackedBookmarkProductInfo();
  }

  trackPriceForBookmark(bookmarkId: bigint) {
    this.handler.trackPriceForBookmark(bookmarkId);
  }

  untrackPriceForBookmark(bookmarkId: bigint) {
    this.handler.untrackPriceForBookmark(bookmarkId);
  }

  static getInstance(): ShoppingListApiProxy {
    return instance || (instance = new ShoppingListApiProxyImpl());
  }

  static setInstance(obj: ShoppingListApiProxy) {
    instance = obj;
  }
}
