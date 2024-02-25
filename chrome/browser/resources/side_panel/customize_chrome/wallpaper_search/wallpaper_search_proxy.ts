// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {WallpaperSearchHandlerInterface} from '../wallpaper_search.mojom-webui.js';
import {WallpaperSearchClientCallbackRouter, WallpaperSearchHandlerFactory, WallpaperSearchHandlerRemote} from '../wallpaper_search.mojom-webui.js';

let instance: WallpaperSearchProxy|null = null;

export class WallpaperSearchProxy {
  static getInstance(): WallpaperSearchProxy {
    if (!instance) {
      const handler = new WallpaperSearchHandlerRemote();
      const callbackRouter = new WallpaperSearchClientCallbackRouter();
      WallpaperSearchHandlerFactory.getRemote().createWallpaperSearchHandler(
          callbackRouter.$.bindNewPipeAndPassRemote(),
          handler.$.bindNewPipeAndPassReceiver());
      instance = new WallpaperSearchProxy(handler, callbackRouter);
    }
    return instance;
  }

  static setInstance(
      handler: WallpaperSearchHandlerInterface,
      callbackRouter: WallpaperSearchClientCallbackRouter) {
    instance = new WallpaperSearchProxy(handler, callbackRouter);
  }

  handler: WallpaperSearchHandlerInterface;
  callbackRouter: WallpaperSearchClientCallbackRouter;

  private constructor(
      handler: WallpaperSearchHandlerInterface,
      callbackRouter: WallpaperSearchClientCallbackRouter) {
    this.handler = handler;
    this.callbackRouter = callbackRouter;
  }
}
