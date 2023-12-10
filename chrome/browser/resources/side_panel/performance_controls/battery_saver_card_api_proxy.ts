// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BatterySaverCardCallbackRouter, BatterySaverCardHandlerFactory, BatterySaverCardHandlerInterface, BatterySaverCardHandlerRemote} from './performance.mojom-webui.js';

let instance: BatterySaverCardApiProxy|null = null;

export interface BatterySaverCardApiProxy {
  getCallbackRouter(): BatterySaverCardCallbackRouter;
}

export class BatterySaverCardApiProxyImpl implements BatterySaverCardApiProxy {
  private callbackRouter: BatterySaverCardCallbackRouter;
  private handler: BatterySaverCardHandlerInterface;

  constructor(
      callbackRouter: BatterySaverCardCallbackRouter,
      handler: BatterySaverCardHandlerInterface) {
    this.callbackRouter = callbackRouter;
    this.handler = handler;
  }

  getCallbackRouter() {
    return this.callbackRouter;
  }

  static getInstance(): BatterySaverCardApiProxy {
    if (!instance) {
      const callbackRouter = new BatterySaverCardCallbackRouter();
      const handler = new BatterySaverCardHandlerRemote();

      const factory = BatterySaverCardHandlerFactory.getRemote();
      factory.createBatterySaverCardHandler(
          callbackRouter.$.bindNewPipeAndPassRemote(),
          handler.$.bindNewPipeAndPassReceiver());
      instance = new BatterySaverCardApiProxyImpl(callbackRouter, handler);
    }
    return instance;
  }

  static setInstance(obj: BatterySaverCardApiProxy) {
    instance = obj;
  }
}
