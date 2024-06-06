// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PageHandlerInterface} from './data_sharing.mojom-webui.js';
import {PageHandlerFactory, PageHandlerRemote} from './data_sharing.mojom-webui.js';

export class BrowserProxy {
  handler: PageHandlerInterface;

  constructor() {
    this.handler = new PageHandlerRemote();

    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        (this.handler as PageHandlerRemote).$.bindNewPipeAndPassReceiver());

    this.handler.showUI();
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxy());
  }

  static setInstance(obj: BrowserProxy) {
    instance = obj;
  }
}

let instance: BrowserProxy|null = null;
