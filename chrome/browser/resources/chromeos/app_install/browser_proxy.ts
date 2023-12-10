// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageHandlerFactory, PageHandlerRemote} from './app_install.mojom-webui.js';

export class BrowserProxy {
  handler: PageHandlerRemote = new PageHandlerRemote();

  constructor() {
    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(this.handler.$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxy());
  }
}

let instance: BrowserProxy|null = null;
