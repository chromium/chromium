// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './omnibox_everywhere.mojom-webui.js';

export interface OmniboxEverywhereBrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerRemote;
}

export class OmniboxEverywhereBrowserProxyImpl implements
    OmniboxEverywhereBrowserProxy {
  callbackRouter: PageCallbackRouter = new PageCallbackRouter();
  handler: PageHandlerRemote = new PageHandlerRemote();

  constructor() {
    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): OmniboxEverywhereBrowserProxy {
    return instance || (instance = new OmniboxEverywhereBrowserProxyImpl());
  }

  static setInstance(proxy: OmniboxEverywhereBrowserProxy) {
    instance = proxy;
  }
}

let instance: OmniboxEverywhereBrowserProxy|null = null;
