// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ContextualTasksToolbarUIObserverCallbackRouter, ContextualTasksToolbarUIService, PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './contextual_tasks_toolbar.mojom-webui.js';
import type {ContextualTasksToolbarUIServiceInterface, PageHandlerInterface} from './contextual_tasks_toolbar.mojom-webui.js';

let instance: ToolbarBrowserProxy|null = null;

export interface ToolbarBrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerInterface;
  // Service used by the toolbar to fetch state and forward chip interactions
  // to the browser.
  toolbarUiService: ContextualTasksToolbarUIServiceInterface;
  // Receives push updates from the browser. Bound to the `updateStream`
  // returned by `toolbarUiService.getInitialState()`.
  toolbarUiObserverCallbackRouter:
      ContextualTasksToolbarUIObserverCallbackRouter;
}

export class ToolbarBrowserProxyImpl implements ToolbarBrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerInterface;
  toolbarUiService: ContextualTasksToolbarUIServiceInterface;
  toolbarUiObserverCallbackRouter:
      ContextualTasksToolbarUIObserverCallbackRouter;

  constructor() {
    this.callbackRouter = new PageCallbackRouter();
    this.handler = new PageHandlerRemote();

    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        (this.handler as PageHandlerRemote).$.bindNewPipeAndPassReceiver());

    this.toolbarUiService = ContextualTasksToolbarUIService.getRemote();
    this.toolbarUiObserverCallbackRouter =
        new ContextualTasksToolbarUIObserverCallbackRouter();
  }

  static getInstance(): ToolbarBrowserProxy {
    return instance || (instance = new ToolbarBrowserProxyImpl());
  }

  static setInstance(proxy: ToolbarBrowserProxy) {
    instance = proxy;
  }
}
