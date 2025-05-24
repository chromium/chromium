// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {NewTabFooterHandlerInterface} from './new_tab_footer.mojom-webui.js';
import {NewTabFooterDocumentCallbackRouter, NewTabFooterHandlerFactory, NewTabFooterHandlerRemote} from './new_tab_footer.mojom-webui.js';

let instance: NewTabFooterDocumentProxy|null = null;

export class NewTabFooterDocumentProxy {
  static getInstance(): NewTabFooterDocumentProxy {
    if (!instance) {
      const handler = new NewTabFooterHandlerRemote();
      const callbackRouter = new NewTabFooterDocumentCallbackRouter();
      const factoryRemote = NewTabFooterHandlerFactory.getRemote();
      factoryRemote.createNewTabFooterHandler(
          callbackRouter.$.bindNewPipeAndPassRemote(),
          handler.$.bindNewPipeAndPassReceiver());
      instance = new NewTabFooterDocumentProxy(handler, callbackRouter);
    }
    return instance;
  }

  static setInstance(
      handler: NewTabFooterHandlerInterface,
      callbackRouter: NewTabFooterDocumentCallbackRouter) {
    instance = new NewTabFooterDocumentProxy(handler, callbackRouter);
  }

  handler: NewTabFooterHandlerInterface;
  callbackRouter: NewTabFooterDocumentCallbackRouter;

  private constructor(
      handler: NewTabFooterHandlerInterface,
      callbackRouter: NewTabFooterDocumentCallbackRouter) {
    this.handler = handler;
    this.callbackRouter = callbackRouter;
  }
}
