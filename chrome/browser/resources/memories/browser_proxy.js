// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import './memories_definition.mojom-lite.js';
import './memories_api.mojom-lite.js';

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

/**
 * @fileoverview This file provides a singleton class that exposes the Mojo
 * handler interface used for bidirectional communication between the page and
 * the browser.
 */

export class BrowserProxy {
  constructor() {
    /** @type {!memories.mojom.PageHandlerRemote} */
    this.handler = memories.mojom.PageHandler.getRemote();

    /** @type {!memories.mojom.PageCallbackRouter} */
    this.callbackRouter = new memories.mojom.PageCallbackRouter();
    this.handler.setPage(this.callbackRouter.$.bindNewPipeAndPassRemote());
  }
}

addSingletonGetter(BrowserProxy);
