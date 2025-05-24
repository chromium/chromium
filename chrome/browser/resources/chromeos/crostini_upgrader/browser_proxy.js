// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './crostini_upgrader.mojom-webui.js';

export class BrowserProxy {
  constructor() {
    /** @type {PageCallbackRouter} */
    this.callbackRouter = new PageCallbackRouter();
    /** @type {PageHandlerRemote} */
    this.handler = new PageHandlerRemote();

    const factory = PageHandlerFactory.getRemote();
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
