// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OnDeviceInternalsPageCallbackRouter, OnDeviceInternalsPageHandlerFactory, OnDeviceInternalsPageHandlerRemote} from './on_device_internals_page.mojom-webui.js';

let instance: BrowserProxy|null = null;

/** Holds Mojo interfaces for communication with the browser process. */
export class BrowserProxy {
  static getInstance(): BrowserProxy {
    if (!instance) {
      const callbackRouter = new OnDeviceInternalsPageCallbackRouter();
      const handler = new OnDeviceInternalsPageHandlerRemote();
      OnDeviceInternalsPageHandlerFactory.getRemote().createPageHandler(
          callbackRouter.$.bindNewPipeAndPassRemote(),
          handler.$.bindNewPipeAndPassReceiver());
      instance = new BrowserProxy(handler, callbackRouter);
    }
    return instance;
  }

  handler: OnDeviceInternalsPageHandlerRemote;
  callbackRouter: OnDeviceInternalsPageCallbackRouter;

  private constructor(
      handler: OnDeviceInternalsPageHandlerRemote,
      callbackRouter: OnDeviceInternalsPageCallbackRouter) {
    this.handler = handler;
    this.callbackRouter = callbackRouter;
  }
}
