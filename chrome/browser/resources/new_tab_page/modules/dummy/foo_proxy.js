// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../foo.mojom-lite.js';

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

/**
 * @fileoverview This file provides a class that exposes the Mojo handler
 * interface used for sending requests from NTP dummy module JS to the browser
 * and receiving the browser response.
 */

export class FooProxy {
  constructor() {
    /** @type {!foo.mojom.FooHandlerRemote} */
    this.handler = foo.mojom.FooHandler.getRemote();
  }
}

addSingletonGetter(FooProxy);