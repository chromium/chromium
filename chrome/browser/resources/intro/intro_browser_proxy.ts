// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {IntroPageCallbackRouter, IntroPageHandlerFactory} from './intro.mojom-webui.js';

export interface IntroBrowserProxy {
  callbackRouter: IntroPageCallbackRouter;
}

export class IntroBrowserProxyImpl implements IntroBrowserProxy {
  callbackRouter: IntroPageCallbackRouter;

  private constructor() {
    this.callbackRouter = new IntroPageCallbackRouter();
    const factory = IntroPageHandlerFactory.getRemote();
    factory.createIntroPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote());
  }

  static getInstance(): IntroBrowserProxy {
    return instance || (instance = new IntroBrowserProxyImpl());
  }

  static setInstance(proxy: IntroBrowserProxy) {
    instance = proxy;
  }
}

let instance: IntroBrowserProxy|null = null;
