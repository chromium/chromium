// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './read_anything.mojom-webui.js';

let instance: ReadAnythingApiProxy|null = null;

export interface ReadAnythingApiProxy {
  getCallbackRouter(): PageCallbackRouter;
  onUIReady(): void;
}

export class ReadAnythingApiProxyImpl implements ReadAnythingApiProxy {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerRemote;

  constructor() {
    this.callbackRouter = new PageCallbackRouter();
    this.handler = new PageHandlerRemote();
    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  getCallbackRouter() {
    return this.callbackRouter;
  }

  onUIReady() {
    this.handler.onUIReady();
  }

  static getInstance(): ReadAnythingApiProxy {
    return instance || (instance = new ReadAnythingApiProxyImpl());
  }

  static setInstance(obj: ReadAnythingApiProxy) {
    instance = obj;
  }
}
