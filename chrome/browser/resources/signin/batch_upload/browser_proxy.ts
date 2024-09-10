// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './batch_upload.mojom-webui.js';
import type {PageHandlerInterface} from './batch_upload.mojom-webui.js';

// Exporting the interface helps when creating a TestBrowserProxy wrapper.
export interface BatchUploadBrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerInterface;
}

export class BatchUploadBrowserProxyImpl implements BatchUploadBrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerInterface;

  private constructor() {
    this.callbackRouter = new PageCallbackRouter();
    this.handler = new PageHandlerRemote();
    PageHandlerFactory.getRemote().createBatchUploadHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        (this.handler as PageHandlerRemote).$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): BatchUploadBrowserProxy {
    return instance || (instance = new BatchUploadBrowserProxyImpl());
  }

  static setInstance(proxy: BatchUploadBrowserProxy) {
    instance = proxy;
  }
}

let instance: BatchUploadBrowserProxy|null = null;
