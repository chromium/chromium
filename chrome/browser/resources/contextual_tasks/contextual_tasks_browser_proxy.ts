// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ExtensionPageCallbackRouter, ExtensionPageHandlerFactory, ExtensionPageHandlerRemote, PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './contextual_tasks.mojom-webui.js';
import type {ExtensionPageHandlerInterface, PageHandlerInterface} from './contextual_tasks.mojom-webui.js';

let instance: BrowserProxy|null = null;

export interface BrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerInterface;
}

export class BrowserProxyImpl implements BrowserProxy {
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

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxyImpl());
  }

  static setInstance(proxy: BrowserProxy) {
    instance = proxy;
  }
}

export interface ExtensionBrowserProxy {
  callbackRouter: ExtensionPageCallbackRouter;
  handler: ExtensionPageHandlerInterface;
}

let extensionInstance: ExtensionBrowserProxy|null = null;

export class ExtensionBrowserProxyImpl implements ExtensionBrowserProxy {
  callbackRouter: ExtensionPageCallbackRouter;
  handler: ExtensionPageHandlerInterface;

  constructor() {
    this.callbackRouter = new ExtensionPageCallbackRouter();
    this.handler = new ExtensionPageHandlerRemote();

    const factory = ExtensionPageHandlerFactory.getRemote();
    factory.createExtensionPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        (this.handler as ExtensionPageHandlerRemote)
            .$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): ExtensionBrowserProxy {
    return extensionInstance ||
        (extensionInstance = new ExtensionBrowserProxyImpl());
  }

  static setInstance(proxy: ExtensionBrowserProxy) {
    extensionInstance = proxy;
  }
}
