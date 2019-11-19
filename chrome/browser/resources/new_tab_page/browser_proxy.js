// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import './new_tab_page.mojom-lite.js';
import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

export class BrowserProxy {
  constructor() {
    /** @type {newTabPage.mojom.PageCallbackRouter} */
    this.callbackRouter = new newTabPage.mojom.PageCallbackRouter();

    /** @type {newTabPage.mojom.PageHandlerRemote} */
    this.handler = new newTabPage.mojom.PageHandlerRemote();

    const factory = newTabPage.mojom.PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }
}

addSingletonGetter(BrowserProxy);
