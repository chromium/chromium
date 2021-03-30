// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../foo.mojom-lite.js';

/**
 * @fileoverview This file provides a class that exposes the Mojo handler
 * interface used for sending requests from NTP dummy module JS to the browser
 * and receiving the browser response.
 */

/** @type {FooProxy} */
let instance = null;

export class FooProxy {
  /** @return {!FooProxy} */
  static getInstance() {
    return instance || (instance = new FooProxy());
  }

  /** @param {FooProxy} newInstance */
  static setInstance(newInstance) {
    instance = newInstance;
  }

  constructor() {
    /** @type {!foo.mojom.FooHandlerRemote} */
    this.handler = foo.mojom.FooHandler.getRemote();
  }
}
