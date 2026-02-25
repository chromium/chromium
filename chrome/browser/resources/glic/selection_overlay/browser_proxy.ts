// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SelectionOverlayPageCallbackRouter, SelectionOverlayPageHandlerFactory, SelectionOverlayPageHandlerRemote} from './selection_overlay.mojom-webui.js';

export class BrowserProxy {
  handler: SelectionOverlayPageHandlerRemote;
  callbackRouter: SelectionOverlayPageCallbackRouter;

  constructor() {
    this.handler = new SelectionOverlayPageHandlerRemote();
    this.callbackRouter = new SelectionOverlayPageCallbackRouter();

    const factory = SelectionOverlayPageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.handler.$.bindNewPipeAndPassReceiver(),
        this.callbackRouter.$.bindNewPipeAndPassRemote());
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxy());
  }
}

let instance: BrowserProxy|null = null;
