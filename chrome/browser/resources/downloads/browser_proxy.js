// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import './downloads.mojom-lite.js';

  export class BrowserProxy {
    constructor() {
      /** @type {downloads.mojom.PageCallbackRouter} */
      this.callbackRouter = new downloads.mojom.PageCallbackRouter();

      /** @type {downloads.mojom.PageHandlerRemote} */
      this.handler = new downloads.mojom.PageHandlerRemote();

      const factory = downloads.mojom.PageHandlerFactory.getRemote();
      factory.createPageHandler(
          this.callbackRouter.$.bindNewPipeAndPassRemote(),
          this.handler.$.bindNewPipeAndPassReceiver());
    }
  }

  addSingletonGetter(BrowserProxy);
