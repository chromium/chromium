// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {browserProxyFactory} from './demo_mode_app_untrusted_ui.mojom-webui.js';

/**
 * Provides interfaces for sending and receiving messages to/from the browser
 * process via Mojo APIs.
 */
class PageHandler {
  handler = browserProxyFactory.getInstance().handler;

  toggleFullscreen() {
    this.handler.toggleFullscreen();
  }

  launchApp(appId: string) {
    this.handler.launchApp(appId);
  }
}

export const pageHandler = new PageHandler();
