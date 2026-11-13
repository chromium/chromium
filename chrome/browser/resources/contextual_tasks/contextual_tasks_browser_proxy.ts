// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ExtensionPageCallbackRouter, ExtensionPageHandlerFactory, ExtensionPageHandlerRemote} from './contextual_tasks.mojom-webui.js';
import type {ExtensionPageHandlerInterface} from './contextual_tasks.mojom-webui.js';

export interface ExtensionBrowserProxy {
  callbackRouter: ExtensionPageCallbackRouter;
  handler: ExtensionPageHandlerInterface;
}

declare global {
  interface Window {
    __extensionBrowserProxyInstance?: ExtensionBrowserProxy;
  }
}

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
    return window.__extensionBrowserProxyInstance ||
        (window.__extensionBrowserProxyInstance =
             new ExtensionBrowserProxyImpl());
  }

  static setInstance(proxy: ExtensionBrowserProxy) {
    window.__extensionBrowserProxyInstance = proxy;
  }
}
