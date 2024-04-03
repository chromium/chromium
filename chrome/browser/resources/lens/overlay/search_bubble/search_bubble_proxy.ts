// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SearchBubblePageCallbackRouter, SearchBubblePageHandlerFactory, SearchBubblePageHandlerRemote} from './search_bubble.mojom-webui.js';

let instance: SearchBubbleProxy|null = null;

export interface SearchBubbleProxy {
  callbackRouter: SearchBubblePageCallbackRouter;
  handler: SearchBubblePageHandlerRemote;
}

export class SearchBubbleProxyImpl implements SearchBubbleProxy {
  callbackRouter: SearchBubblePageCallbackRouter;
  handler: SearchBubblePageHandlerRemote;

  constructor() {
    this.callbackRouter = new SearchBubblePageCallbackRouter();
    this.handler = new SearchBubblePageHandlerRemote();

    const factoryRemote = SearchBubblePageHandlerFactory.getRemote();
    factoryRemote.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  static getInstance() {
    return instance || (instance = new SearchBubbleProxyImpl());
  }

  static setInstance(obj: SearchBubbleProxy) {
    instance = obj;
  }
}
