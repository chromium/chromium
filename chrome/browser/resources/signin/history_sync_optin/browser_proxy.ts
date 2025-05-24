// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './history_sync_optin.mojom-webui.js';
import type {PageHandlerInterface} from './history_sync_optin.mojom-webui.js';

// Exporting the interface helps when creating a TestBrowserProxy wrapper.
export interface HistorySyncOptInBrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerInterface;
}

export class HistorySyncOptInBrowserProxyImpl implements
    HistorySyncOptInBrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerInterface;
  private constructor() {
    this.callbackRouter = new PageCallbackRouter();
    this.handler = new PageHandlerRemote();
    PageHandlerFactory.getRemote().createHistorySyncOptinHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        (this.handler as PageHandlerRemote).$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): HistorySyncOptInBrowserProxy {
    return instance || (instance = new HistorySyncOptInBrowserProxyImpl());
  }

  static setInstance(proxy: HistorySyncOptInBrowserProxy) {
    instance = proxy;
  }
}

let instance: HistorySyncOptInBrowserProxy|null = null;
