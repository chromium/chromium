// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/1179821): Migrate to JS module Mojo bindings.
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/url/mojom/url.mojom-lite.js';
import './chrome_cart.mojom-lite.js';

/**
 * @fileoverview This file provides a class that exposes the Mojo handler
 * interface used for sending requests from NTP chrome cart module JS to the
 * browser and receiving the browser response.
 */

/** @type {?chromeCart.mojom.CartHandlerRemote} */
let handler = null;

export class ChromeCartProxy {
  /** @return {!chromeCart.mojom.CartHandlerRemote} */
  static getHandler() {
    return handler || (handler = chromeCart.mojom.CartHandler.getRemote());
  }

  /** @param {!chromeCart.mojom.CartHandlerRemote} newHandler */
  static setHandler(newHandler) {
    handler = newHandler;
  }

  /** @private */
  constructor() {}
}
