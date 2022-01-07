// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FooHandler, FooHandlerRemote} from '../../foo.mojom-webui.js';

/**
 * @fileoverview This file provides a class that exposes the Mojo handler
 * interface used for sending requests from NTP dummy module JS to the browser
 * and receiving the browser response.
 */

/** @type {?FooHandlerRemote} */
let handler = null;

export class FooProxy {
  /** @return {!FooHandlerRemote} */
  static getHandler() {
    return handler || (handler = FooHandler.getRemote());
  }

  /** @param {!FooHandlerRemote} newHandler */
  static setHandler(newHandler) {
    handler = newHandler;
  }

  /** @private */
  constructor() {}
}
