// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './local_files_migration.mojom-webui.js';

export abstract class LocalFilesBrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerRemote;

  constructor() {
    this.callbackRouter = new PageCallbackRouter();
    this.handler = new PageHandlerRemote();
  }

  static getInstance(): LocalFilesBrowserProxy {
    return instance || (instance = new LocalFilesBrowserProxyImpl());
  }

  static setInstance(proxy: LocalFilesBrowserProxy) {
    instance = proxy;
  }
}

class LocalFilesBrowserProxyImpl extends LocalFilesBrowserProxy {
  constructor() {
    super();
    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }
}

let instance: LocalFilesBrowserProxy|null = null;
