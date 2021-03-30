// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './chrome_cart.mojom-lite.js';

/**
 * @fileoverview This file provides a class that exposes the Mojo handler
 * interface used for sending requests from NTP chrome cart module JS to the
 * browser and receiving the browser response.
 */

/** @type {ChromeCartProxy} */
let instance = null;

export class ChromeCartProxy {
  /** @return {!ChromeCartProxy} */
  static getInstance() {
    return instance || (instance = new ChromeCartProxy());
  }

  /** @param {ChromeCartProxy} newInstance */
  static setInstance(newInstance) {
    instance = newInstance;
  }

  constructor() {
    /** @type {!chromeCart.mojom.CartHandlerRemote} */
    this.handler = chromeCart.mojom.CartHandler.getRemote();
  }
}
