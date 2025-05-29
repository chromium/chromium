
// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomizeButtonsDocumentCallbackRouter, CustomizeButtonsHandlerFactory, CustomizeButtonsHandlerRemote} from './customize_buttons.mojom-webui.js';

let instance: CustomizeButtonsProxy|null = null;

/** Holds Mojo interfaces for communication with the browser process. */
export class CustomizeButtonsProxy {
  static getInstance(): CustomizeButtonsProxy {
    if (!instance) {
      const handler = new CustomizeButtonsHandlerRemote();
      const callbackRouter = new CustomizeButtonsDocumentCallbackRouter();
      CustomizeButtonsHandlerFactory.getRemote().createCustomizeButtonsHandler(
          callbackRouter.$.bindNewPipeAndPassRemote(),
          handler.$.bindNewPipeAndPassReceiver());
      instance = new CustomizeButtonsProxy(handler, callbackRouter);
    }
    return instance;
  }

  static setInstance(
      handler: CustomizeButtonsHandlerRemote,
      callbackRouter: CustomizeButtonsDocumentCallbackRouter) {
    instance = new CustomizeButtonsProxy(handler, callbackRouter);
  }

  handler: CustomizeButtonsHandlerRemote;
  callbackRouter: CustomizeButtonsDocumentCallbackRouter;

  private constructor(
      handler: CustomizeButtonsHandlerRemote,
      callbackRouter: CustomizeButtonsDocumentCallbackRouter) {
    this.handler = handler;
    this.callbackRouter = callbackRouter;
  }
}
