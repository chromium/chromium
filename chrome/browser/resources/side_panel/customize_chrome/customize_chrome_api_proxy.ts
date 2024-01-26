// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CustomizeChromePageHandlerInterface} from './customize_chrome.mojom-webui.js';
import {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerFactory, CustomizeChromePageHandlerRemote} from './customize_chrome.mojom-webui.js';

let instance: CustomizeChromeApiProxy|null = null;

export class CustomizeChromeApiProxy {
  static getInstance(): CustomizeChromeApiProxy {
    if (!instance) {
      const handler = new CustomizeChromePageHandlerRemote();
      const callbackRouter = new CustomizeChromePageCallbackRouter();
      CustomizeChromePageHandlerFactory.getRemote().createPageHandler(
          callbackRouter.$.bindNewPipeAndPassRemote(),
          handler.$.bindNewPipeAndPassReceiver());
      instance = new CustomizeChromeApiProxy(handler, callbackRouter);
    }
    return instance;
  }

  static setInstance(
      handler: CustomizeChromePageHandlerInterface,
      callbackRouter: CustomizeChromePageCallbackRouter) {
    instance = new CustomizeChromeApiProxy(handler, callbackRouter);
  }

  handler: CustomizeChromePageHandlerInterface;
  callbackRouter: CustomizeChromePageCallbackRouter;

  private constructor(
      handler: CustomizeChromePageHandlerInterface,
      callbackRouter: CustomizeChromePageCallbackRouter) {
    this.handler = handler;
    this.callbackRouter = callbackRouter;
  }
}
