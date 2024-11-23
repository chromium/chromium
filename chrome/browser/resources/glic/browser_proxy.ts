// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageHandlerFactory, PageHandlerRemote} from './glic.mojom-webui.js';
import type {PageHandlerInterface} from './glic.mojom-webui.js';

export interface BrowserProxy {
  handler: PageHandlerInterface;
}

let instance: BrowserProxy|null = null;

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
}
