// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory} from './launcher_internals.mojom-webui.js';

export class BrowserProxy {
  callbackRouter: PageCallbackRouter = new PageCallbackRouter();

  constructor() {
    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(this.callbackRouter.$.bindNewPipeAndPassRemote());
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxy());
  }

  static setInstance(obj: BrowserProxy) {
    instance = obj;
  }
}

let instance: BrowserProxy|null = null;
