// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MicrosoftAuthUntrustedDocumentCallbackRouter} from './ntp_microsoft_auth_shared_ui.mojom-webui.js';
import {MicrosoftAuthUntrustedDocumentInterfacesFactory} from './ntp_microsoft_auth_untrusted_ui.mojom-webui.js';

let instance: MicrosoftAuthUntrustedDocumentProxy|null = null;

export class MicrosoftAuthUntrustedDocumentProxy {
  static getInstance(): MicrosoftAuthUntrustedDocumentProxy {
    if (!instance) {
      const callbackRouter = new MicrosoftAuthUntrustedDocumentCallbackRouter();
      const factoryRemote =
          MicrosoftAuthUntrustedDocumentInterfacesFactory.getRemote();
      factoryRemote.connectToParentDocument(
          callbackRouter.$.bindNewPipeAndPassRemote());
      instance = new MicrosoftAuthUntrustedDocumentProxy(callbackRouter);
    }
    return instance;
  }

  static setInstance(callbackRouter:
                         MicrosoftAuthUntrustedDocumentCallbackRouter) {
    instance = new MicrosoftAuthUntrustedDocumentProxy(callbackRouter);
  }

  callbackRouter: MicrosoftAuthUntrustedDocumentCallbackRouter;

  private constructor(callbackRouter:
                          MicrosoftAuthUntrustedDocumentCallbackRouter) {
    this.callbackRouter = callbackRouter;
  }
}
