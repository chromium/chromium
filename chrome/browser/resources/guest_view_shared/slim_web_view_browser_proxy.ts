// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageHandlerFactory, PageHandlerRemote} from './slim_web_view.mojom-webui.js';
import type {PageHandlerInterface} from './slim_web_view.mojom-webui.js';

export interface BrowserProxy {
  handler: PageHandlerInterface;
}

export class BrowserProxyImpl implements BrowserProxy {
  handler: PageHandlerInterface;

  private constructor() {
    this.handler = new PageHandlerRemote();
    PageHandlerFactory.getRemote().createPageHandler(
        (this.handler as PageHandlerRemote).$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxyImpl());
  }

  static setInstance(proxy: BrowserProxy) {
    instance = proxy;
  }
}

let instance: BrowserProxy|null = null;
