// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ModelBrokerDebugRemote} from './model_broker_debug.mojom-webui.js';
import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './on_device_internals_page.mojom-webui.js';

let instance: BrowserProxy|null = null;

/** Holds Mojo interfaces for communication with the browser process. */
export class BrowserProxy {
  static getInstance(): BrowserProxy {
    if (!instance) {
      const callbackRouter = new PageCallbackRouter();
      const handler = new PageHandlerRemote();
      const brokerDebug = new ModelBrokerDebugRemote();
      PageHandlerFactory.getRemote().createPageHandler(
          callbackRouter.$.bindNewPipeAndPassRemote(),
          handler.$.bindNewPipeAndPassReceiver());
      handler.bindModelBrokerDebug(brokerDebug.$.bindNewPipeAndPassReceiver());
      instance = new BrowserProxy(handler, callbackRouter, brokerDebug);
    }
    return instance;
  }

  handler: PageHandlerRemote;
  callbackRouter: PageCallbackRouter;
  brokerDebug: ModelBrokerDebugRemote;

  private constructor(
      handler: PageHandlerRemote, callbackRouter: PageCallbackRouter,
      brokerDebug: ModelBrokerDebugRemote) {
    this.handler = handler;
    this.callbackRouter = callbackRouter;
    this.brokerDebug = brokerDebug;
  }
}
