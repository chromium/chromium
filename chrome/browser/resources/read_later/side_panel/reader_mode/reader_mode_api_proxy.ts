// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './reader_mode.mojom-webui.js';

let instance: ReaderModeApiProxy|null = null;

export class ReaderModeApiProxy {
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

  showUI() {
    this.handler.showUI();
  }

  static getInstance() {
    return instance || (instance = new ReaderModeApiProxy());
  }

  static setInstance(obj: ReaderModeApiProxy) {
    instance = obj;
  }
}
