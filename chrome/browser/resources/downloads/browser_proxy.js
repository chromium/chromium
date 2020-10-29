// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './downloads.mojom-webui.js';

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
}

addSingletonGetter(BrowserProxy);
