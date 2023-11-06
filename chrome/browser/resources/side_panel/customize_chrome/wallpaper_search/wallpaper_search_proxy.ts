// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {WallpaperSearchHandler, WallpaperSearchHandlerInterface} from '../wallpaper_search.mojom-webui.js';

let handler: WallpaperSearchHandlerInterface|null = null;

export class WallpaperSearchProxy {
  static getHandler(): WallpaperSearchHandlerInterface {
    return handler || (handler = WallpaperSearchHandler.getRemote());
  }

  static setHandler(newHandler: WallpaperSearchHandlerInterface) {
    handler = newHandler;
  }

  private constructor() {}
}
