// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from '../sign_in_celebration.mojom-webui.js';
import type {PageHandlerInterface} from '../sign_in_celebration.mojom-webui.js';

export interface SignInCelebrationBrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerInterface;

  matchMedia(query: string): MediaQueryList;
}

export class SignInCelebrationBrowserProxyImpl implements
    SignInCelebrationBrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerInterface;

  private constructor() {
    this.callbackRouter = new PageCallbackRouter();
    this.handler = new PageHandlerRemote();

    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        (this.handler as PageHandlerRemote).$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): SignInCelebrationBrowserProxy {
    return instance || (instance = new SignInCelebrationBrowserProxyImpl());
  }

  static setInstance(proxy: SignInCelebrationBrowserProxy) {
    instance = proxy;
  }

  matchMedia(query: string): MediaQueryList {
    return window.matchMedia(query);
  }
}

let instance: SignInCelebrationBrowserProxy|null = null;
