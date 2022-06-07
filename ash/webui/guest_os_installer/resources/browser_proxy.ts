// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import './guest_os_installer.mojom-lite.js';

export class BrowserProxy {
  // The mojo generator doesn't generate typescript bindings yet, so
  // we can't name the types these should have.
  callbackRouter: any;
  handler: any;

  constructor() {
    this.callbackRouter = new ash.guestOsInstaller.mojom.PageCallbackRouter();
    this.handler = new ash.guestOsInstaller.mojom.PageHandlerRemote();

    const factory = ash.guestOsInstaller.mojom.PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  static instance: BrowserProxy|null = null;
  static getInstance() {
    return BrowserProxy.instance ||
        (BrowserProxy.instance = new BrowserProxy());
  }
}
