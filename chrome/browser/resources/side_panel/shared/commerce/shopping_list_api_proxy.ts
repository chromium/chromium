// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(b:283833590): Rename this file since it serves for all shopping features
// now.
import {BookmarkProductInfo, PageCallbackRouter, PriceInsightsInfo, ProductInfo, ShoppingListHandlerFactory, ShoppingListHandlerRemote} from '../shopping_list.mojom-webui.js';

let instance: ShoppingListApiProxy|null = null;

export interface ShoppingListApiProxy {
  getAllPriceTrackedBookmarkProductInfo():
      Promise<{productInfos: BookmarkProductInfo[]}>;
  getAllShoppingBookmarkProductInfo():
      Promise<{productInfos: BookmarkProductInfo[]}>;
  trackPriceForBookmark(bookmarkId: bigint): void;
  untrackPriceForBookmark(bookmarkId: bigint): void;
  getProductInfoForCurrentUrl(): Promise<{productInfo: ProductInfo}>;
  getPriceInsightsInfoForCurrentUrl():
      Promise<{priceInsightsInfo: PriceInsightsInfo}>;
  showInsightsSidePanelUi(): void;
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

  getProductInfoForCurrentUrl() {
    return this.handler.getProductInfoForCurrentUrl();
  }

  getPriceInsightsInfoForCurrentUrl() {
    return this.handler.getPriceInsightsInfoForCurrentUrl();
  }

  showInsightsSidePanelUi() {
    this.handler.showInsightsSidePanelUI();
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
