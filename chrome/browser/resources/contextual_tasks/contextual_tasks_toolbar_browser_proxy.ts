// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './contextual_tasks_toolbar.mojom-webui.js';
import type {PageHandlerInterface} from './contextual_tasks_toolbar.mojom-webui.js';

let instance: ToolbarBrowserProxy|null = null;

export interface ToolbarBrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerInterface;
}

export class ToolbarBrowserProxyImpl implements ToolbarBrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerInterface;

  constructor() {
    this.callbackRouter = new PageCallbackRouter();
    this.handler = new PageHandlerRemote();

    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        (this.handler as PageHandlerRemote).$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): ToolbarBrowserProxy {
    return instance || (instance = new ToolbarBrowserProxyImpl());
  }

  static setInstance(proxy: ToolbarBrowserProxy) {
    instance = proxy;
  }
}
