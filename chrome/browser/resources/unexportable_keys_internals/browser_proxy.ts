// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './unexportable_keys_internals.mojom-webui.js';
import type {PageHandlerInterface} from './unexportable_keys_internals.mojom-webui.js';

// Exporting the interface helps when creating a TestBrowserProxy wrapper.
export interface UnexportableKeysInternalsBrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerInterface;
}

export class UnexportableKeysInternalsBrowserProxyImpl implements
    UnexportableKeysInternalsBrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerInterface;

  private constructor() {
    this.callbackRouter = new PageCallbackRouter();
    this.handler = new PageHandlerRemote();
    PageHandlerFactory.getRemote().createUnexportableKeysInternalsHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        (this.handler as PageHandlerRemote).$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): UnexportableKeysInternalsBrowserProxy {
    return instance ||
        (instance = new UnexportableKeysInternalsBrowserProxyImpl());
  }

  static setInstance(proxy: UnexportableKeysInternalsBrowserProxy) {
    instance = proxy;
  }
}

let instance: UnexportableKeysInternalsBrowserProxy|null = null;
