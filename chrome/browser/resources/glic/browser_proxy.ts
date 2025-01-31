// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FrePageHandlerRemote, PageHandlerFactory, PageHandlerRemote} from './glic.mojom-webui.js';
import type {FrePageHandlerInterface, PageHandlerInterface} from './glic.mojom-webui.js';

export interface BrowserProxy {
  handler: PageHandlerInterface;
  freHandler: FrePageHandlerInterface;
}

let instance: BrowserProxy|null = null;

export class BrowserProxyImpl implements BrowserProxy {
  handler: PageHandlerInterface;
  freHandler: FrePageHandlerInterface;

  private constructor() {
    this.handler = new PageHandlerRemote();
    PageHandlerFactory.getRemote().createPageHandler(
        (this.handler as PageHandlerRemote).$.bindNewPipeAndPassReceiver());

    // TODO(crbug.com/393399675): Separate out FRE logic
    this.freHandler = new FrePageHandlerRemote();
    PageHandlerFactory.getRemote().createFrePageHandler(
        (this.freHandler as FrePageHandlerRemote)
            .$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxyImpl());
  }
}
