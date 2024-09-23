// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote, PageRemote} from '../mojom-webui/date_time_handler.mojom-webui.js';

// Communicates with the DateTimeHandler in the browser process.
class DateTimeBrowserProxy {
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

  static getInstance(): DateTimeBrowserProxy {
    return instance || (instance = new DateTimeBrowserProxy());
  }

  static setInstanceForTesting(proxy: DateTimeBrowserProxy): void {
    instance = proxy;
  }
}

let instance: DateTimeBrowserProxy|null = null;

export {
  DateTimeBrowserProxy,
  PageRemote as DateTimePageRemote,
  PageHandlerRemote as DateTimePageHandlerRemote,
  PageCallbackRouter as DateTimePageCallbackRouter,
};
