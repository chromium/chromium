// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../foo.mojom-lite.js';

/**
 * @fileoverview This file provides a class that exposes the Mojo handler
 * interface used for sending requests from NTP dummy module JS to the browser
 * and receiving the browser response.
 */

/** @type {?foo.mojom.FooHandlerRemote} */
let handler = null;

export class FooProxy {
  /** @return {!foo.mojom.FooHandlerRemote} */
  static getHandler() {
    return handler || (handler = foo.mojom.FooHandler.getRemote());
  }

  /** @param {!foo.mojom.FooHandlerRemote} newHandler */
  static setHandler(newHandler) {
    handler = newHandler;
  }

  /** @private */
  constructor() {}
}
