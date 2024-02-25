// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote, PageRemote, Status} from '../mojom-webui/google_drive_handler.mojom-webui.js';
import {Stage} from '../mojom-webui/pinning_manager_types.mojom-webui.js';

// Communicates with the GoogleDrivePageHandler in the browser process.
class GoogleDriveBrowserProxy {
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

  static getInstance(): GoogleDriveBrowserProxy {
    return instance || (instance = new GoogleDriveBrowserProxy());
  }

  static setInstance(proxy: GoogleDriveBrowserProxy): void {
    instance = proxy;
  }
}

let instance: GoogleDriveBrowserProxy|null = null;

export {
  GoogleDriveBrowserProxy,
  PageHandlerRemote as GoogleDrivePageHandlerRemote,
  PageCallbackRouter as GoogleDrivePageCallbackRouter,
  PageRemote as GoogleDrivePageRemote,
  Stage,
  Status,
};
