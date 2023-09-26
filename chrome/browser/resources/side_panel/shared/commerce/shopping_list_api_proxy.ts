// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(b:283833590): Rename this file since it serves for all shopping features
// now.
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

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
  isShoppingListEligible(): Promise<{eligible: boolean}>;
  getShoppingCollectionBookmarkFolderId(): Promise<{collectionId: bigint}>;
  getPriceTrackingStatusForCurrentUrl(): Promise<{tracked: boolean}>;
  setPriceTrackingStatusForCurrentUrl(track: boolean): void;
  openUrlInNewTab(url: Url): void;
  getParentBookmarkFolderNameForCurrentUrl(): Promise<{name: String16}>;
  showBookmarkEditorForCurrentUrl(): void;
  showFeedback(): void;
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

  isShoppingListEligible() {
    return this.handler.isShoppingListEligible();
  }

  getShoppingCollectionBookmarkFolderId() {
    return this.handler.getShoppingCollectionBookmarkFolderId();
  }

  getPriceTrackingStatusForCurrentUrl() {
    return this.handler.getPriceTrackingStatusForCurrentUrl();
  }

  setPriceTrackingStatusForCurrentUrl(track: boolean) {
    this.handler.setPriceTrackingStatusForCurrentUrl(track);
  }

  openUrlInNewTab(url: Url) {
    this.handler.openUrlInNewTab(url);
  }

  getParentBookmarkFolderNameForCurrentUrl() {
    return this.handler.getParentBookmarkFolderNameForCurrentUrl();
  }

  showBookmarkEditorForCurrentUrl() {
    this.handler.showBookmarkEditorForCurrentUrl();
  }

  showFeedback() {
    this.handler.showFeedback();
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
