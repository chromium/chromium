// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageHandlerFactory, PageHandlerRemote} from '/ash/webui/demo_mode_app_ui/mojom/demo_mode_app_ui.mojom-webui.js';

/**
 * Provides interfaces for sending and receiving messages to/from the browser
 * process via Mojo APIs.
 */
class PageHandler {
  constructor() {
    this.handler = new PageHandlerRemote();

    const factoryRemote = PageHandlerFactory.getRemote();
    factoryRemote.createPageHandler(
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  toggleFullscreen() {
    this.handler.toggleFullscreen();
  }
}

export const pageHandler = new PageHandler();
