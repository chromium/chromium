// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './on_device_translation_internals.mojom-webui.js';

export class OnDeviceTranslationInternalsBrowserProxy {
  private handler: PageHandlerRemote;
  private callbackRouter: PageCallbackRouter;

  constructor() {
    this.callbackRouter = new PageCallbackRouter();
    this.handler = new PageHandlerRemote();
    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  installLanguagePackage(packageIndex: number) {
    return this.handler.installLanguagePackage(packageIndex);
  }
  uninstallLanguagePackage(packageIndex: number) {
    return this.handler.uninstallLanguagePackage(packageIndex);
  }

  static getInstance(): OnDeviceTranslationInternalsBrowserProxy {
    return instance ||
        (instance = new OnDeviceTranslationInternalsBrowserProxy());
  }

  getCallbackRouter(): PageCallbackRouter {
    return this.callbackRouter;
  }
}

let instance: OnDeviceTranslationInternalsBrowserProxy|null = null;
