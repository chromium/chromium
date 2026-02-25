// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './slim_web_view.mojom-webui.js';
import type {PageHandlerInterface} from './slim_web_view.mojom-webui.js';

export {PageHandler_PermissionResponseAction as PermissionResponseAction} from './slim_web_view.mojom-webui.js';

export interface BrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerInterface;
}

export class BrowserProxyImpl implements BrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerInterface;

  private constructor() {
    this.callbackRouter = new PageCallbackRouter();
    this.handler = new PageHandlerRemote();
    PageHandlerFactory.getRemote().createPageHandler(
        (this.handler as PageHandlerRemote).$.bindNewPipeAndPassReceiver(),
        this.callbackRouter.$.bindNewPipeAndPassRemote());
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxyImpl());
  }

  static setInstance(proxy: BrowserProxy) {
    instance = proxy;
  }
}

let instance: BrowserProxy|null = null;
