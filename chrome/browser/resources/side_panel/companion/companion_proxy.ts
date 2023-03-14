// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CompanionPageCallbackRouter, CompanionPageHandlerFactory, CompanionPageHandlerRemote} from './companion.mojom-webui.js';

let instance: CompanionProxy|null = null;

export interface CompanionProxy {
  callbackRouter: CompanionPageCallbackRouter;
  handler: CompanionPageHandlerRemote;
}

export class CompanionProxyImpl implements CompanionProxy {
  callbackRouter: CompanionPageCallbackRouter;
  handler: CompanionPageHandlerRemote;

  constructor() {
    this.callbackRouter = new CompanionPageCallbackRouter();
    this.handler = new CompanionPageHandlerRemote();

    // Use the CompanionPageHandlerFactory to create a connection to the page
    // handler.
    const factoryRemote = CompanionPageHandlerFactory.getRemote();
    factoryRemote.createCompanionPageHandler(
        this.handler.$.bindNewPipeAndPassReceiver(),
        this.callbackRouter.$.bindNewPipeAndPassRemote());
  }

  static getInstance() {
    return instance || (instance = new CompanionProxyImpl());
  }

  static setInstance(obj: CompanionProxy) {
    instance = obj;
  }
}
