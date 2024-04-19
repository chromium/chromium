// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CustomizeToolbarHandlerInterface} from '../customize_toolbar.mojom-webui.js';
import {CustomizeToolbarClientCallbackRouter, CustomizeToolbarHandlerFactory, CustomizeToolbarHandlerRemote} from '../customize_toolbar.mojom-webui.js';

let instance: CustomizeToolbarApiProxy|null = null;

export class CustomizeToolbarApiProxy {
  static getInstance(): CustomizeToolbarApiProxy {
    if (!instance) {
      const handler = new CustomizeToolbarHandlerRemote();
      const callbackRouter = new CustomizeToolbarClientCallbackRouter();
      CustomizeToolbarHandlerFactory.getRemote().createCustomizeToolbarHandler(
          callbackRouter.$.bindNewPipeAndPassRemote(),
          handler.$.bindNewPipeAndPassReceiver());
      instance = new CustomizeToolbarApiProxy(handler, callbackRouter);
    }
    return instance;
  }

  static setInstance(
      handler: CustomizeToolbarHandlerInterface,
      callbackRouter: CustomizeToolbarClientCallbackRouter) {
    instance = new CustomizeToolbarApiProxy(handler, callbackRouter);
  }

  handler: CustomizeToolbarHandlerInterface;
  callbackRouter: CustomizeToolbarClientCallbackRouter;

  private constructor(
      handler: CustomizeToolbarHandlerInterface,
      callbackRouter: CustomizeToolbarClientCallbackRouter) {
    this.handler = handler;
    this.callbackRouter = callbackRouter;
  }
}
