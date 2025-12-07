// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './comments.mojom-webui.js';
import type {PageHandlerInterface} from './comments.mojom-webui.js';

let instance: CommentsApiProxy|null = null;

export interface CommentsApiProxy {
  pageCallbackRouter: PageCallbackRouter;
  handler: PageHandlerInterface;

  showUi(): void;
}

export class CommentsApiProxyImpl implements CommentsApiProxy {
  pageCallbackRouter: PageCallbackRouter;
  handler: PageHandlerRemote;

  constructor() {
    this.pageCallbackRouter = new PageCallbackRouter();
    this.handler = new PageHandlerRemote();

    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.pageCallbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  showUi() {
    this.handler.showUI();
  }

  static getInstance(): CommentsApiProxy {
    return instance || (instance = new CommentsApiProxyImpl());
  }

  static setInstance(obj: CommentsApiProxy) {
    instance = obj;
  }
}
