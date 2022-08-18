// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BookmarkProductInfo, ShoppingListHandlerFactory, ShoppingListHandlerRemote} from './shopping_list.mojom-webui.js';

let instance: ShoppingListApiProxy|null = null;

export interface ShoppingListApiProxy {
  getAllBookmarkProductInfo(): Promise<{productInfos: BookmarkProductInfo[]}>;
}

export class ShoppingListApiProxyImpl implements ShoppingListApiProxy {
  handler: ShoppingListHandlerRemote;

  constructor() {
    this.handler = new ShoppingListHandlerRemote();

    const factory = ShoppingListHandlerFactory.getRemote();
    factory.createShoppingListHandler(
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  getAllBookmarkProductInfo() {
    return this.handler.getAllBookmarkProductInfo();
  }

  static getInstance(): ShoppingListApiProxy {
    return instance || (instance = new ShoppingListApiProxyImpl());
  }

  static setInstance(obj: ShoppingListApiProxy) {
    instance = obj;
  }
}
