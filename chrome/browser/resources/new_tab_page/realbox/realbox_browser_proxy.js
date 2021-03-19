// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './realbox.mojom-lite.js';

/**
 * @fileoverview This file provides a singleton class that exposes the Mojo
 * handler interface used for bidirectional communication between the NTP
 * realbox JS and the browser.
 */

/** @type {RealboxBrowserProxy} */
let instance = null;

export class RealboxBrowserProxy {
  /** @return {!RealboxBrowserProxy} */
  static getInstance() {
    return instance || (instance = new RealboxBrowserProxy());
  }

  /** @param {RealboxBrowserProxy} newInstance */
  static setInstance(newInstance) {
    instance = newInstance;
  }

  constructor() {
    /** @type {!realbox.mojom.PageHandlerRemote} */
    this.handler = realbox.mojom.PageHandler.getRemote();

    /** @type {!realbox.mojom.PageCallbackRouter} */
    this.callbackRouter = new realbox.mojom.PageCallbackRouter();

    this.handler.setPage(this.callbackRouter.$.bindNewPipeAndPassRemote());
  }
}
