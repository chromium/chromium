// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BaseDialogPageHandlerInterface} from './base_dialog.mojom-webui.js';
import {BaseDialogPageCallbackRouter, BaseDialogPageHandlerFactory, BaseDialogPageHandlerRemote} from './base_dialog.mojom-webui.js';

export class BaseDialogBrowserProxy {
  callbackRouter: BaseDialogPageCallbackRouter;
  handler: BaseDialogPageHandlerInterface;

  // Creates communication pipes for both the remote and the receiver.
  // 1. Constructs a valid PendingRemote to send messages to the
  // `callbackRouter`.
  // 2. Constructs a valid PendingReceiver on the existing Remote
  // (BaseDialogPageHandlerRemote) to accept BaseDialogPageHandler interface
  // calls.
  constructor() {
    this.callbackRouter = new BaseDialogPageCallbackRouter();
    this.handler = new BaseDialogPageHandlerRemote();
    BaseDialogPageHandlerFactory.getRemote().createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        (this.handler as BaseDialogPageHandlerRemote)
            .$.bindNewPipeAndPassReceiver());
  }

  static setInstance(proxy: BaseDialogBrowserProxy) {
    instance = proxy;
  }

  static getInstance(): BaseDialogBrowserProxy {
    return instance || (instance = new BaseDialogBrowserProxy());
  }
}

let instance: BaseDialogBrowserProxy|null = null;
