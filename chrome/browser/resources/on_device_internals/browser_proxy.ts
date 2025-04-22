// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './on_device_internals_page.mojom-webui.js';

let instance: BrowserProxy|null = null;

/** Holds Mojo interfaces for communication with the browser process. */
export class BrowserProxy {
  static getInstance(): BrowserProxy {
    if (!instance) {
      const callbackRouter = new PageCallbackRouter();
      const handler = new PageHandlerRemote();
      PageHandlerFactory.getRemote().createPageHandler(
          callbackRouter.$.bindNewPipeAndPassRemote(),
          handler.$.bindNewPipeAndPassReceiver());
      instance = new BrowserProxy(handler, callbackRouter);
    }
    return instance;
  }

  handler: PageHandlerRemote;
  callbackRouter: PageCallbackRouter;

  private constructor(
      handler: PageHandlerRemote, callbackRouter: PageCallbackRouter) {
    this.handler = handler;
    this.callbackRouter = callbackRouter;
  }
}
