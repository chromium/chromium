// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './file_manager.mojom-webui.js';

export class BrowserProxy {
  constructor() {
    /** @type {!PageCallbackRouter} */
    this.callbackRouter = new PageCallbackRouter();
    /** @type {!PageHandlerRemote} */
    this.handler = new PageHandlerRemote();

    const factoryRemote = PageHandlerFactory.getRemote();
    factoryRemote.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }
}
