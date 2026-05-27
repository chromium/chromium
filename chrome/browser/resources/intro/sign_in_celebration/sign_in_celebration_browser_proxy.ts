// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SignInCelebrationPageCallbackRouter, SignInCelebrationPageHandlerFactory, SignInCelebrationPageHandlerRemote} from '../sign_in_celebration.mojom-webui.js';
import type {SignInCelebrationPageHandlerInterface} from '../sign_in_celebration.mojom-webui.js';

export interface SignInCelebrationBrowserProxy {
  callbackRouter: SignInCelebrationPageCallbackRouter;
  handler: SignInCelebrationPageHandlerInterface;

  matchMedia(query: string): MediaQueryList;
}

export class SignInCelebrationBrowserProxyImpl implements
    SignInCelebrationBrowserProxy {
  callbackRouter: SignInCelebrationPageCallbackRouter;
  handler: SignInCelebrationPageHandlerInterface;

  private constructor() {
    this.callbackRouter = new SignInCelebrationPageCallbackRouter();
    this.handler = new SignInCelebrationPageHandlerRemote();

    const factory = SignInCelebrationPageHandlerFactory.getRemote();
    factory.createSignInCelebrationPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        (this.handler as SignInCelebrationPageHandlerRemote)
            .$.bindNewPipeAndPassReceiver());
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
