// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {LensPageHandlerInterface} from './lens.mojom-webui.js';
import {LensPageCallbackRouter, LensPageHandlerFactory, LensPageHandlerRemote} from './lens.mojom-webui.js';

let instance: BrowserProxy|null = null;

export interface BrowserProxy {
  callbackRouter: LensPageCallbackRouter;
  handler: LensPageHandlerInterface;
}

export class BrowserProxyImpl implements BrowserProxy {
  callbackRouter: LensPageCallbackRouter = new LensPageCallbackRouter();
  handler: LensPageHandlerRemote = new LensPageHandlerRemote();

  constructor() {
    const factory = LensPageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.handler.$.bindNewPipeAndPassReceiver(),
        this.callbackRouter.$.bindNewPipeAndPassRemote());
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxyImpl());
  }

  static setInstance(obj: BrowserProxy) {
    instance = obj;
  }
}
