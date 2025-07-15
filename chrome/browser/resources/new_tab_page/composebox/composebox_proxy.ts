// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from '../composebox.mojom-webui.js';

export interface ComposeboxProxy {
  handler: PageHandlerRemote;
  callbackRouter: PageCallbackRouter;
}

export class ComposeboxProxyImpl implements ComposeboxProxy {
  handler: PageHandlerRemote;
  callbackRouter: PageCallbackRouter;

  constructor(handler: PageHandlerRemote, callbackRouter: PageCallbackRouter) {
    this.handler = handler;
    this.callbackRouter = callbackRouter;
  }

  static getInstance(): ComposeboxProxyImpl {
    if (instance) {
      return instance;
    }
    const callbackRouter = new PageCallbackRouter();
    const handler = new PageHandlerRemote();
    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        callbackRouter.$.bindNewPipeAndPassRemote(),
        handler.$.bindNewPipeAndPassReceiver());
    instance = new ComposeboxProxyImpl(handler, callbackRouter);
    return instance;
  }

  static setInstance(newInstance: ComposeboxProxy) {
    instance = newInstance;
  }
}

let instance: ComposeboxProxy|null = null;
