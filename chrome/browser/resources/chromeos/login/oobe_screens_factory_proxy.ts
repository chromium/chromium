// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ScreensFactory, ScreensFactoryRemote} from './mojom-webui/screens_factory.mojom-webui.js';

// Communicates with the OobeScreensHandlerFactory in the browser process.
class OobeScreensFacotryBrowserProxy {
  screenFactory: ScreensFactoryRemote;

  constructor() {
    this.screenFactory = ScreensFactory.getRemote();
  }

  static getInstance(): OobeScreensFacotryBrowserProxy {
    return instance || (instance = new OobeScreensFacotryBrowserProxy());
  }

  static setInstance(proxy: OobeScreensFacotryBrowserProxy): void {
    instance = proxy;
  }
}

let instance: OobeScreensFacotryBrowserProxy|null = null;

export {
  OobeScreensFacotryBrowserProxy,
};
