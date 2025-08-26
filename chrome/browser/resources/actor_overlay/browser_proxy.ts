// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ActorOverlayPageHandlerInterface} from './actor_overlay.mojom-webui.js';
import {ActorOverlayPageCallbackRouter, ActorOverlayPageHandlerFactory, ActorOverlayPageHandlerRemote} from './actor_overlay.mojom-webui.js';

export class ActorOverlayBrowserProxy {
  callbackRouter: ActorOverlayPageCallbackRouter;
  handler: ActorOverlayPageHandlerInterface;

  constructor() {
    this.callbackRouter = new ActorOverlayPageCallbackRouter();
    this.handler = new ActorOverlayPageHandlerRemote();
    ActorOverlayPageHandlerFactory.getRemote().createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        (this.handler as ActorOverlayPageHandlerRemote)
            .$.bindNewPipeAndPassReceiver());
  }

  static setInstance(proxy: ActorOverlayBrowserProxy) {
    instance = proxy;
  }

  static getInstance(): ActorOverlayBrowserProxy {
    return instance || (instance = new ActorOverlayBrowserProxy());
  }
}

let instance: ActorOverlayBrowserProxy|null = null;
