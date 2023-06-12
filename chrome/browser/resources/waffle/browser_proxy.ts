// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the chrome://waffle page to
 * interact with the browser.
 */

import {PageHandlerFactory, PageHandlerInterface, PageHandlerRemote} from './waffle.mojom-webui.js';

export interface WaffleChoice {
  name: string;
}

export class WaffleBrowserProxy {
  handler: PageHandlerInterface;

  constructor() {
    this.handler = new PageHandlerRemote();

    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        (this.handler as PageHandlerRemote).$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): WaffleBrowserProxy {
    return instance || (instance = new WaffleBrowserProxy());
  }

  static setInstance(obj: WaffleBrowserProxy) {
    instance = obj;
  }
}

let instance: WaffleBrowserProxy|null = null;
