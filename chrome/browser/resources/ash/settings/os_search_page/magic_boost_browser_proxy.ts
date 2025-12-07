// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageHandlerFactory, PageHandlerRemote} from '../mojom-webui/magic_boost_handler.mojom-webui.js';

// Communicates with the MagicBoostNoticeBrowserProxy in the browser process.
class MagicBoostNoticeBrowserProxy {
  // Invoke methods from the browser process.
  private handler: PageHandlerRemote = new PageHandlerRemote();

  constructor() {
    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(this.handler.$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): MagicBoostNoticeBrowserProxy {
    return instance || (instance = new MagicBoostNoticeBrowserProxy());
  }

  static setInstance(proxy: MagicBoostNoticeBrowserProxy): void {
    instance = proxy;
  }

  showNotice(): void {
    this.handler.showNotice();
  }
}

let instance: MagicBoostNoticeBrowserProxy|null = null;

export {MagicBoostNoticeBrowserProxy};
