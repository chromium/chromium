// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {HarnessPageCallbackRouter, HarnessPageHandlerFactory, HarnessPageHandlerRemote} from '../eval_harness.mojom-webui.js';

export class EvalProxy {
  handler: HarnessPageHandlerRemote;
  callbackRouter: HarnessPageCallbackRouter;

  constructor() {
    this.handler = new HarnessPageHandlerRemote();
    this.callbackRouter = new HarnessPageCallbackRouter();
    const factory = HarnessPageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.handler.$.bindNewPipeAndPassReceiver(),
        this.callbackRouter.$.bindNewPipeAndPassRemote());
  }

  static getInstance(): EvalProxy {
    return instance || (instance = new EvalProxy());
  }

  static setInstance(obj: EvalProxy) {
    instance = obj;
  }
}

let instance: EvalProxy|null = null;
