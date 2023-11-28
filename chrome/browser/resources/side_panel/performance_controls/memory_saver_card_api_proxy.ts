// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MemorySaverCardCallbackRouter, MemorySaverCardHandlerFactory, MemorySaverCardHandlerInterface, MemorySaverCardHandlerRemote} from './performance.mojom-webui.js';

let instance: MemorySaverCardApiProxy|null = null;

export interface MemorySaverCardApiProxy {
  getCallbackRouter(): MemorySaverCardCallbackRouter;
}

export class MemorySaverCardApiProxyImpl implements MemorySaverCardApiProxy {
  private callbackRouter: MemorySaverCardCallbackRouter;
  private handler: MemorySaverCardHandlerInterface;

  constructor(
      callbackRouter: MemorySaverCardCallbackRouter,
      handler: MemorySaverCardHandlerInterface) {
    this.callbackRouter = callbackRouter;
    this.handler = handler;
  }

  getCallbackRouter() {
    return this.callbackRouter;
  }

  static getInstance(): MemorySaverCardApiProxy {
    if (!instance) {
      const callbackRouter = new MemorySaverCardCallbackRouter();
      const handler = new MemorySaverCardHandlerRemote();

      const factory = MemorySaverCardHandlerFactory.getRemote();
      factory.createMemorySaverCardHandler(
          callbackRouter.$.bindNewPipeAndPassRemote(),
          handler.$.bindNewPipeAndPassReceiver());
      instance = new MemorySaverCardApiProxyImpl(callbackRouter, handler);
    }
    return instance;
  }

  static setInstance(obj: MemorySaverCardApiProxy) {
    instance = obj;
  }
}
