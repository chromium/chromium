// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandler, PageHandlerRemote} from '/chrome/browser/ui/webui/memories/memories.mojom-webui.js';
import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

/**
 * @fileoverview This file provides a singleton class that exposes the Mojo
 * handler interface used for bidirectional communication between the page and
 * the browser.
 */

export class BrowserProxy {
  constructor() {
    /** @type {!PageHandlerRemote} */
    this.handler = PageHandler.getRemote();

    /** @type {!PageCallbackRouter} */
    this.callbackRouter = new PageCallbackRouter();
    this.handler.setPage(this.callbackRouter.$.bindNewPipeAndPassRemote());
  }
}

addSingletonGetter(BrowserProxy);
