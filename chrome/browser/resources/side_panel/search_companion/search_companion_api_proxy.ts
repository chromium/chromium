// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SearchCompanionPageCallbackRouter, SearchCompanionPageHandlerFactory, SearchCompanionPageHandlerRemote} from './search_companion.mojom-webui.js';

let instance: SearchCompanionApiProxy|null = null;

export interface SearchCompanionApiProxy {
  callbackRouter: SearchCompanionPageCallbackRouter;
  showUi(): void;
}

export class SearchCompanionApiProxyImpl implements SearchCompanionApiProxy {
  handler: SearchCompanionPageHandlerRemote;
  callbackRouter: SearchCompanionPageCallbackRouter;

  constructor() {
    this.handler = new SearchCompanionPageHandlerRemote();
    this.callbackRouter = new SearchCompanionPageCallbackRouter();

    const factory = SearchCompanionPageHandlerFactory.getRemote();
    factory.createSearchCompanionPageHandler(
        this.handler.$.bindNewPipeAndPassReceiver(),
        this.callbackRouter.$.bindNewPipeAndPassRemote());
  }

  showUi() {
    this.handler.showUI();
  }

  static getInstance() {
    return instance || (instance = new SearchCompanionApiProxyImpl());
  }

  static setInstance(obj: SearchCompanionApiProxy) {
    instance = obj;
  }
}