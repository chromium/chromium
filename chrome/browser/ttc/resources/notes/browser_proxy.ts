// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from '../ai_overlay_dialog.mojom-webui.js';
import {AiOverlayToolsRemote} from '../tools.mojom-webui.js';

export class BrowserProxy {
  handler: PageHandlerRemote;

  constructor() {
    this.handler = new PageHandlerRemote();
    const factory = PageHandlerFactory.getRemote();
    const page = new PageCallbackRouter();
    const tools = new AiOverlayToolsRemote();
    factory.createPageHandler(
        this.handler.$.bindNewPipeAndPassReceiver(),
        page.$.bindNewPipeAndPassRemote(),
        tools.$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxy());
  }

  static setInstance(obj: BrowserProxy) {
    instance = obj;
  }
}

let instance: BrowserProxy|null = null;
