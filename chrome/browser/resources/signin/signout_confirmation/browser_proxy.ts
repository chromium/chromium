// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './signout_confirmation.mojom-webui.js';
import type {PageHandlerInterface} from './signout_confirmation.mojom-webui.js';

// Exporting the interface helps when creating a TestBrowserProxy wrapper.
export interface SignoutConfirmationBrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerInterface;
}

export class SignoutConfirmationBrowserProxyImpl implements
    SignoutConfirmationBrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerInterface;

  private constructor() {
    this.callbackRouter = new PageCallbackRouter();
    this.handler = new PageHandlerRemote();
    PageHandlerFactory.getRemote().createSignoutConfirmationHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        (this.handler as PageHandlerRemote).$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): SignoutConfirmationBrowserProxy {
    return instance || (instance = new SignoutConfirmationBrowserProxyImpl());
  }

  static setInstance(proxy: SignoutConfirmationBrowserProxy) {
    instance = proxy;
  }
}

let instance: SignoutConfirmationBrowserProxy|null = null;
