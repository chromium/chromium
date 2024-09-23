// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ScreensFactory, ScreensFactoryRemote} from './mojom-webui/screens_factory.mojom-webui.js';

// Communicates with the OobeScreensHandlerFactory in the browser process.
class OobeScreensFactoryBrowserProxy {
  screenFactory: ScreensFactoryRemote;

  constructor() {
    this.screenFactory = ScreensFactory.getRemote();
  }

  static getInstance(): OobeScreensFactoryBrowserProxy {
    return instance || (instance = new OobeScreensFactoryBrowserProxy());
  }

  static setInstance(proxy: OobeScreensFactoryBrowserProxy): void {
    instance = proxy;
  }
}

let instance: OobeScreensFactoryBrowserProxy|null = null;

export {
  OobeScreensFactoryBrowserProxy,
};
