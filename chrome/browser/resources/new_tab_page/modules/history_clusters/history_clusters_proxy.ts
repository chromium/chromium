// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandler, PageHandlerRemote} from 'chrome://resources/cr_components/history_clusters/history_clusters.mojom-webui.js';

export interface HistoryClustersProxy {
  handler: PageHandlerRemote;
  callbackRouter: PageCallbackRouter;
}

export class HistoryClustersProxyImpl implements HistoryClustersProxy {
  handler: PageHandlerRemote;
  callbackRouter: PageCallbackRouter;

  constructor(handler: PageHandlerRemote, callbackRouter: PageCallbackRouter) {
    this.handler = handler;
    this.callbackRouter = callbackRouter;
  }

  static getInstance(): HistoryClustersProxy {
    if (instance) {
      return instance;
    }

    const handler = PageHandler.getRemote();
    const callbackRouter = new PageCallbackRouter();
    handler.setPage(callbackRouter.$.bindNewPipeAndPassRemote());
    return instance = new HistoryClustersProxyImpl(handler, callbackRouter);
  }

  static setInstance(obj: HistoryClustersProxy) {
    instance = obj;
  }
}

let instance: HistoryClustersProxy|null = null;
