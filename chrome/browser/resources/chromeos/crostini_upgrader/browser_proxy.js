// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(lxj): use es6 module when it is ready crbug/1004256
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import './crostini_upgrader.mojom-lite.js';

export class BrowserProxy {
  constructor() {
    /** @type {ash.crostiniUpgrader.mojom.PageCallbackRouter} */
    this.callbackRouter = new ash.crostiniUpgrader.mojom.PageCallbackRouter();
    /** @type {ash.crostiniUpgrader.mojom.PageHandlerRemote} */
    this.handler = new ash.crostiniUpgrader.mojom.PageHandlerRemote();

    const factory = ash.crostiniUpgrader.mojom.PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  /** @return {!BrowserProxy} */
  static getInstance() {
    return instance || (instance = new BrowserProxy());
  }

  /** @param {!BrowserProxy} obj */
  static setInstance(obj) {
    instance = obj;
  }
}

/** @type {?BrowserProxy} */
let instance = null;
