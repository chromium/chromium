// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './whats_new.mojom-webui.js';
import type {PageHandlerInterface} from './whats_new.mojom-webui.js';

export interface WhatsNewProxy {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerInterface;
}

export class WhatsNewProxyImpl implements WhatsNewProxy {
  handler: PageHandlerInterface;
  callbackRouter: PageCallbackRouter;

  private constructor() {
    this.handler = new PageHandlerRemote();
    this.callbackRouter = new PageCallbackRouter();
    PageHandlerFactory.getRemote().createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        (this.handler as PageHandlerRemote).$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): WhatsNewProxy {
    return instance || (instance = new WhatsNewProxyImpl());
  }

  static setInstance(proxy: WhatsNewProxy) {
    instance = proxy;
  }
}

let instance: WhatsNewProxy|null = null;
