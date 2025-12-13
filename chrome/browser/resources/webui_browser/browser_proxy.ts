// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './browser.mojom-webui.js';

export class BrowserProxy {
  private handler: PageHandlerRemote;
  callbackRouter: PageCallbackRouter = new PageCallbackRouter();

  constructor() {
    this.handler = new PageHandlerRemote();
    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  static getCallbackRouter(): PageCallbackRouter {
    return BrowserProxy.getInstance().callbackRouter;
  }

  static getPageHandler(): PageHandlerRemote {
    return BrowserProxy.getInstance().handler;
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxy());
  }
}

let instance: BrowserProxy|null = null;
