// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CartHandler, CartHandlerRemote} from './chrome_cart.mojom-webui.js';

/**
 * @fileoverview This file provides a class that exposes the Mojo handler
 * interface used for sending requests from NTP chrome cart module JS to the
 * browser and receiving the browser response.
 */

let handler: CartHandlerRemote|null = null;

export class ChromeCartProxy {
  static getHandler(): CartHandlerRemote {
    return handler || (handler = CartHandler.getRemote());
  }

  static setHandler(newHandler: CartHandlerRemote) {
    handler = newHandler;
  }

  private constructor() {}
}
