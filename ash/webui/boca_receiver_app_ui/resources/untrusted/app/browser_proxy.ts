// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {UntrustedPageCallbackRouter, UntrustedPageHandlerFactory} from '../mojom/boca_receiver.mojom-webui.js';

// Exporting the interface helps when creating a TestBrowserProxy wrapper.
export interface BrowserProxy {
  callbackRouter: UntrustedPageCallbackRouter;
}

export class BrowserProxyImpl {
  readonly callbackRouter: UntrustedPageCallbackRouter;

  private constructor() {
    this.callbackRouter = new UntrustedPageCallbackRouter();
    UntrustedPageHandlerFactory.getRemote().createUntrustedPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote());
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxyImpl());
  }

  static setInstanceForTest(proxy: BrowserProxy) {
    instance = proxy;
  }
}

let instance: BrowserProxy|null = null;
