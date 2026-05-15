// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './multistep_filter_internals.mojom-webui.js';

export class BrowserProxyImpl {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerRemote;

  private constructor() {
    this.callbackRouter = new PageCallbackRouter();
    this.handler = new PageHandlerRemote();
    PageHandlerFactory.getRemote().createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): BrowserProxyImpl {
    return instance || (instance = new BrowserProxyImpl());
  }

  static setInstance(proxy: BrowserProxyImpl) {
    instance = proxy;
  }
}

let instance: BrowserProxyImpl|null = null;
