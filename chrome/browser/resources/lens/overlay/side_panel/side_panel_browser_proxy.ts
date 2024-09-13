// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {LensSidePanelPageHandlerInterface} from '../lens.mojom-webui.js';
import {LensSidePanelPageCallbackRouter, LensSidePanelPageHandlerFactory, LensSidePanelPageHandlerRemote} from '../lens.mojom-webui.js';

let instance: SidePanelBrowserProxy|null = null;

export interface SidePanelBrowserProxy {
  callbackRouter: LensSidePanelPageCallbackRouter;
  handler: LensSidePanelPageHandlerInterface;
}

export class SidePanelBrowserProxyImpl implements SidePanelBrowserProxy {
  callbackRouter: LensSidePanelPageCallbackRouter =
      new LensSidePanelPageCallbackRouter();
  handler: LensSidePanelPageHandlerRemote =
      new LensSidePanelPageHandlerRemote();

  constructor() {
    const factory = LensSidePanelPageHandlerFactory.getRemote();
    factory.createSidePanelPageHandler(
        this.handler.$.bindNewPipeAndPassReceiver(),
        this.callbackRouter.$.bindNewPipeAndPassRemote());
  }

  static getInstance(): SidePanelBrowserProxy {
    return instance || (instance = new SidePanelBrowserProxyImpl());
  }

  static setInstance(obj: SidePanelBrowserProxy) {
    instance = obj;
  }
}
