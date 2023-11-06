// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PerformancePageCallbackRouter, PerformancePageHandlerFactory, PerformancePageHandlerInterface, PerformancePageHandlerRemote} from './performance.mojom-webui.js';

let instance: PerformanceApiProxy|null = null;

export interface PerformanceApiProxy {
  getCallbackRouter(): PerformancePageCallbackRouter;
  showUi(): void;
}

export class PerformanceApiProxyImpl implements PerformanceApiProxy {
  private callbackRouter: PerformancePageCallbackRouter;
  private handler: PerformancePageHandlerInterface;

  constructor(
      callbackRouter: PerformancePageCallbackRouter,
      handler: PerformancePageHandlerInterface) {
    this.callbackRouter = callbackRouter;
    this.handler = handler;
  }

  showUi() {
    this.handler.showUI();
  }

  getCallbackRouter() {
    return this.callbackRouter;
  }

  static getInstance(): PerformanceApiProxy {
    if (!instance) {
      const callbackRouter = new PerformancePageCallbackRouter();
      const handler = new PerformancePageHandlerRemote();

      const factory = PerformancePageHandlerFactory.getRemote();
      factory.createPerformancePageHandler(
          callbackRouter.$.bindNewPipeAndPassRemote(),
          handler.$.bindNewPipeAndPassReceiver());
      instance = new PerformanceApiProxyImpl(callbackRouter, handler);
    }
    return instance;
  }

  static setInstance(obj: PerformanceApiProxy) {
    instance = obj;
  }
}
