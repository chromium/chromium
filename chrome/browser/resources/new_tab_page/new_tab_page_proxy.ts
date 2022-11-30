// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './new_tab_page.mojom-webui.js';

let instance: NewTabPageProxy|null = null;

/** Holds Mojo interfaces for communication with the browser process. */
export class NewTabPageProxy {
  static getInstance(): NewTabPageProxy {
    if (!instance) {
      const handler = new PageHandlerRemote();
      const callbackRouter = new PageCallbackRouter();
      PageHandlerFactory.getRemote().createPageHandler(
          callbackRouter.$.bindNewPipeAndPassRemote(),
          handler.$.bindNewPipeAndPassReceiver());
      instance = new NewTabPageProxy(handler, callbackRouter);
    }
    return instance;
  }

  static setInstance(
      handler: PageHandlerRemote, callbackRouter: PageCallbackRouter) {
    instance = new NewTabPageProxy(handler, callbackRouter);
  }

  handler: PageHandlerRemote;
  callbackRouter: PageCallbackRouter;

  private constructor(
      handler: PageHandlerRemote, callbackRouter: PageCallbackRouter) {
    this.handler = handler;
    this.callbackRouter = callbackRouter;
  }
}
