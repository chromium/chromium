// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './omnibox_popup.mojom-webui.js';
import type {PageHandlerInterface} from './omnibox_popup.mojom-webui.js';

export class OmniboxPopupBrowserProxy {
  callbackRouter: PageCallbackRouter = new PageCallbackRouter();
  handler: PageHandlerInterface = new PageHandlerRemote();

  private constructor() {
    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        (this.handler as PageHandlerRemote).$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): OmniboxPopupBrowserProxy {
    return instance || (instance = new OmniboxPopupBrowserProxy());
  }

  static setInstance(obj: OmniboxPopupBrowserProxy) {
    instance = obj;
  }
}

let instance: OmniboxPopupBrowserProxy|null = null;
