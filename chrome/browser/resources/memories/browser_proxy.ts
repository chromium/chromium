// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandler, PageHandlerRemote} from './chrome/browser/ui/webui/history_clusters/history_clusters.mojom-webui.js';

/**
 * @fileoverview This file provides a singleton class that exposes the Mojo
 * handler interface used for bidirectional communication between the page and
 * the browser.
 */

export class BrowserProxy {
  handler: PageHandlerRemote;
  callbackRouter: PageCallbackRouter;

  constructor() {
    this.handler = PageHandler.getRemote();
    this.callbackRouter = new PageCallbackRouter();
    this.handler.setPage(this.callbackRouter.$.bindNewPipeAndPassRemote());
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxy());
  }

  static setInstance(obj: BrowserProxy) {
    instance = obj;
  }
}

let instance: BrowserProxy|null = null;
