// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CloseReason, ComposeDialogCallbackRouter, ComposeDialogClosePageHandlerRemote, ComposeDialogPageHandlerFactory, ComposeDialogPageHandlerRemote, StyleModifiers} from './compose.mojom-webui.js';

/** @interface */
export interface ComposeApiProxy {
  compose(style: StyleModifiers, input: string): void;
  getRouter(): ComposeDialogCallbackRouter;
  acceptComposeResult(): void;
}

export class ComposeApiProxyImpl implements ComposeApiProxy {
  static instance: ComposeApiProxy|null = null;

  composeDialogPageHandler = new ComposeDialogPageHandlerRemote();
  composeDialogClosePageHandler = new ComposeDialogClosePageHandlerRemote();
  router = new ComposeDialogCallbackRouter();

  constructor() {
    const factoryRemote = ComposeDialogPageHandlerFactory.getRemote();
    factoryRemote.createComposeDialogPageHandler(
        this.composeDialogClosePageHandler.$.bindNewPipeAndPassReceiver(),
        this.composeDialogPageHandler.$.bindNewPipeAndPassReceiver(),
        this.router.$.bindNewPipeAndPassRemote());
  }

  static getInstance(): ComposeApiProxy {
    return ComposeApiProxyImpl.instance ||
        (ComposeApiProxyImpl.instance = new ComposeApiProxyImpl());
  }

  static setInstance(newInstance: ComposeApiProxy) {
    ComposeApiProxyImpl.instance = newInstance;
  }

  /** @override */
  compose(style: StyleModifiers, input: string): void {
    this.composeDialogPageHandler.compose(style, input);
  }

  /** @override */
  getRouter() {
    return this.router;
  }

  /** @override */
  acceptComposeResult() {
    this.composeDialogPageHandler.acceptComposeResult();
  }

  /** @override */
  closeUi(reason: CloseReason) {
    this.composeDialogClosePageHandler.closeUI(reason);
  }
}
