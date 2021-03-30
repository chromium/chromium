// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/1179821): Migrate to JS module Mojo bindings.
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/text_direction.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-lite.js';
import 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-lite.js';
import 'chrome://resources/mojo/url/mojom/url.mojom-lite.js';

import './realbox/omnibox.mojom-lite.js';
import './realbox/realbox.mojom-lite.js';
import './new_tab_page.mojom-lite.js';

/** @type {?NewTabPageProxy} */
let instance = null;

/** Holds Mojo interfaces for communication with the browser process. */
export class NewTabPageProxy {
  /** @return {!NewTabPageProxy} */
  static getInstance() {
    if (!instance) {
      const handler = new newTabPage.mojom.PageHandlerRemote();
      const callbackRouter = new newTabPage.mojom.PageCallbackRouter();
      newTabPage.mojom.PageHandlerFactory.getRemote().createPageHandler(
          callbackRouter.$.bindNewPipeAndPassRemote(),
          handler.$.bindNewPipeAndPassReceiver());
      instance = new NewTabPageProxy(handler, callbackRouter);
    }
    return instance;
  }

  /**
   * @param {!newTabPage.mojom.PageHandlerRemote} handler
   * @param {!newTabPage.mojom.PageCallbackRouter} callbackRouter
   */
  static setInstance(handler, callbackRouter) {
    instance = new NewTabPageProxy(handler, callbackRouter);
  }

  /**
   * @param {!newTabPage.mojom.PageHandlerRemote} handler
   * @param {!newTabPage.mojom.PageCallbackRouter} callbackRouter
   * @private
   */
  constructor(handler, callbackRouter) {
    this.handler = handler;
    this.callbackRouter = callbackRouter;
  }
}
