// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PageHandlerInterface} from './data_sharing.mojom-webui.js';
import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './data_sharing.mojom-webui.js';
import type {Code} from './data_sharing_sdk_types.js';

export interface BrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler?: PageHandlerInterface;
  showUi(): void;
  closeUi(status: Code): void;
}

export class BrowserProxyImpl implements BrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerInterface;

  constructor() {
    this.callbackRouter = new PageCallbackRouter();
    this.handler = new PageHandlerRemote();

    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        (this.handler as PageHandlerRemote).$.bindNewPipeAndPassReceiver());
  }

  showUi() {
    this.handler.showUI();
  }

  closeUi(status: Code) {
    this.handler.closeUI(status);
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxyImpl());
  }

  static setInstance(obj: BrowserProxy|null) {
    instance = obj;
  }
}

let instance: BrowserProxy|null = null;
