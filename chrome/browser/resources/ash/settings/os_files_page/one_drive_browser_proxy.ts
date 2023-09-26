// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote, PageRemote} from '../mojom-webui/one_drive_handler.mojom-webui.js';

// Communicates with the OneDriveHandler in the browser process.
class OneDriveBrowserProxy {
  // Invoke methods from the browser process.
  handler: PageHandlerRemote = new PageHandlerRemote();

  // Receive updates from the browser process.
  observer: PageCallbackRouter = new PageCallbackRouter();

  constructor() {
    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.observer.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): OneDriveBrowserProxy {
    return instance || (instance = new OneDriveBrowserProxy());
  }

  static setInstance(proxy: OneDriveBrowserProxy): void {
    instance = proxy;
  }
}

let instance: OneDriveBrowserProxy|null = null;

export {
  OneDriveBrowserProxy,
  PageHandlerRemote as OneDrivePageHandlerRemote,
  PageRemote as OneDrivePageRemote,
  PageCallbackRouter as OneDrivePageCallbackRouter,
};
