// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageHandlerFactory, PageHandlerRemote, PageReceiver} from './glic.mojom-webui.js';
import type {PageHandlerInterface, PageInterface} from './glic.mojom-webui.js';

export interface BrowserProxy {
  handler: PageHandlerInterface;
}

export class BrowserProxyImpl implements BrowserProxy {
  handler: PageHandlerInterface;
  constructor(pageInterface: PageInterface) {
    const pageReceiver = new PageReceiver(pageInterface);
    this.handler = new PageHandlerRemote();
    PageHandlerFactory.getRemote().createPageHandler(
        (this.handler as PageHandlerRemote).$.bindNewPipeAndPassReceiver(),
        pageReceiver.$.bindNewPipeAndPassRemote());
  }
}
