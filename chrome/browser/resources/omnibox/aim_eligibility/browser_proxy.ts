// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './aim_eligibility.mojom-webui.js';

export class BrowserProxy {
  private handler: PageHandlerRemote;
  private callbackRouter: PageCallbackRouter;

  constructor() {
    this.handler = new PageHandlerRemote();
    this.callbackRouter = new PageCallbackRouter();
    PageHandlerFactory.getRemote().createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  getCallbackRouter(): PageCallbackRouter {
    return this.callbackRouter;
  }

  getPageHandler(): PageHandlerRemote {
    return this.handler;
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxy());
  }
}

let instance: BrowserProxy|null = null;
