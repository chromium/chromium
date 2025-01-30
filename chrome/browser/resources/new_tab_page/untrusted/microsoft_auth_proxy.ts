// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MicrosoftAuthUntrustedDocumentCallbackRouter} from './ntp_microsoft_auth_shared_ui.mojom-webui.js';
import {MicrosoftAuthUntrustedDocumentInterfacesFactory, MicrosoftAuthUntrustedPageHandlerRemote} from './ntp_microsoft_auth_untrusted_ui.mojom-webui.js';

let instance: MicrosoftAuthUntrustedDocumentProxy|null = null;

export class MicrosoftAuthUntrustedDocumentProxy {
  static getInstance(): MicrosoftAuthUntrustedDocumentProxy {
    if (!instance) {
      const handler = new MicrosoftAuthUntrustedPageHandlerRemote();
      const callbackRouterToParent =
          new MicrosoftAuthUntrustedDocumentCallbackRouter();
      const callbackRouterToHandler =
          new MicrosoftAuthUntrustedDocumentCallbackRouter();
      const factoryRemote =
          MicrosoftAuthUntrustedDocumentInterfacesFactory.getRemote();
      factoryRemote.connectToParentDocument(
          callbackRouterToParent.$.bindNewPipeAndPassRemote());
      factoryRemote.createPageHandler(
          handler.$.bindNewPipeAndPassReceiver(),
          callbackRouterToHandler.$.bindNewPipeAndPassRemote());
      instance = new MicrosoftAuthUntrustedDocumentProxy(
          callbackRouterToParent, callbackRouterToHandler, handler);
    }
    return instance;
  }

  static setInstance(
      callbackRouterToParent: MicrosoftAuthUntrustedDocumentCallbackRouter,
      callbackRouterToHandler: MicrosoftAuthUntrustedDocumentCallbackRouter,
      handler: MicrosoftAuthUntrustedPageHandlerRemote) {
    instance = new MicrosoftAuthUntrustedDocumentProxy(
        callbackRouterToParent, callbackRouterToHandler, handler);
  }

  callbackRouterToParent: MicrosoftAuthUntrustedDocumentCallbackRouter;
  callbackRouterToHandler: MicrosoftAuthUntrustedDocumentCallbackRouter;
  handler: MicrosoftAuthUntrustedPageHandlerRemote;

  private constructor(
      callbackRouterToParent: MicrosoftAuthUntrustedDocumentCallbackRouter,
      callbackRouterToHandler: MicrosoftAuthUntrustedDocumentCallbackRouter,
      handler: MicrosoftAuthUntrustedPageHandlerRemote) {
    this.callbackRouterToParent = callbackRouterToParent;
    this.callbackRouterToHandler = callbackRouterToHandler;
    this.handler = handler;
  }
}
