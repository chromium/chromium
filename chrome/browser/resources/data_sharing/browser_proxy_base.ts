// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PageHandlerInterface} from './data_sharing.mojom-webui.js';
import {PageHandlerFactory, PageHandlerRemote} from './data_sharing.mojom-webui.js';

// Base class of BrowserProxy.
// Implement common logic i.e. setting mojom pipe.
export class BrowserProxyBase {
  handler: PageHandlerInterface;

  constructor() {
    this.handler = new PageHandlerRemote();

    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        (this.handler as PageHandlerRemote).$.bindNewPipeAndPassReceiver());
  }
}
