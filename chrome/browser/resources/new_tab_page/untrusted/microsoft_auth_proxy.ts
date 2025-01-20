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
      const callbackRouter = new MicrosoftAuthUntrustedDocumentCallbackRouter();
      const factoryRemote =
          MicrosoftAuthUntrustedDocumentInterfacesFactory.getRemote();
      factoryRemote.connectToParentDocument(
          callbackRouter.$.bindNewPipeAndPassRemote());
      factoryRemote.createPageHandler(handler.$.bindNewPipeAndPassReceiver());
      instance =
          new MicrosoftAuthUntrustedDocumentProxy(callbackRouter, handler);
    }
    return instance;
  }

  static setInstance(
      callbackRouter: MicrosoftAuthUntrustedDocumentCallbackRouter,
      handler: MicrosoftAuthUntrustedPageHandlerRemote) {
    instance = new MicrosoftAuthUntrustedDocumentProxy(callbackRouter, handler);
  }

  callbackRouter: MicrosoftAuthUntrustedDocumentCallbackRouter;
  handler: MicrosoftAuthUntrustedPageHandlerRemote;

  private constructor(
      callbackRouter: MicrosoftAuthUntrustedDocumentCallbackRouter,
      handler: MicrosoftAuthUntrustedPageHandlerRemote) {
    this.callbackRouter = callbackRouter;
    this.handler = handler;
  }
}
