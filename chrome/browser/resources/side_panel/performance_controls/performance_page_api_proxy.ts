// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PerformancePageCallbackRouter, PerformancePageHandlerFactory, PerformancePageHandlerInterface, PerformancePageHandlerRemote} from './performance.mojom-webui.js';
let instance: PerformancePageApiProxy | null = null;

export interface PerformancePageApiProxy {
  getCallbackRouter(): PerformancePageCallbackRouter;
  showUi(): void;
}

export class PerformancePageApiProxyImpl implements PerformancePageApiProxy {
  private callbackRouter: PerformancePageCallbackRouter;
  private handler: PerformancePageHandlerInterface;

  constructor(
    callbackRouter: PerformancePageCallbackRouter,
    handler: PerformancePageHandlerInterface,
  ) {
    this.callbackRouter = callbackRouter;
    this.handler = handler;
  }

  getCallbackRouter() {
    return this.callbackRouter;
  }

  showUi() {
    this.handler.showUI();
  }

  static getInstance(): PerformancePageApiProxy {
    if (!instance) {
      const callbackRouter = new PerformancePageCallbackRouter();
      const handler = new PerformancePageHandlerRemote();

      const factory = PerformancePageHandlerFactory.getRemote();
      factory.createPerformancePageHandler(
        callbackRouter.$.bindNewPipeAndPassRemote(),
        handler.$.bindNewPipeAndPassReceiver());
      instance = new PerformancePageApiProxyImpl(
        callbackRouter, handler);
    }
    return instance;
  }

  static setInstance(obj: PerformancePageApiProxy) {
    instance = obj;
  }
}
