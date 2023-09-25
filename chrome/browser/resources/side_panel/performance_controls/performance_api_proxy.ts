// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PerformancePageCallbackRouter, PerformancePageHandlerFactory, PerformancePageHandlerRemote} from './performance.mojom-webui.js';

let instance: PerformanceApiProxy|null = null;

export interface PerformanceApiProxy {
  getCallbackRouter(): PerformancePageCallbackRouter;
}

export class PerformanceApiProxyImpl implements PerformanceApiProxy {
  private callbackRouter: PerformancePageCallbackRouter =
      new PerformancePageCallbackRouter();
  private handler: PerformancePageHandlerRemote =
      new PerformancePageHandlerRemote();

  constructor() {
    const factory = PerformancePageHandlerFactory.getRemote();
    factory.createPerformancePageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  getCallbackRouter() {
    return this.callbackRouter;
  }

  static getInstance(): PerformanceApiProxy {
    return instance || (instance = new PerformanceApiProxyImpl());
  }

  static setInstance(obj: PerformanceApiProxy) {
    instance = obj;
  }
}
