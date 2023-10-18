// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ComposeDialogCallbackRouter, ComposeDialogPageHandlerFactory, ComposeDialogPageHandlerInterface, ComposeDialogPageHandlerRemote, StyleModifiers} from './compose.mojom-webui.js';

/** @interface */
export interface ComposeApiProxy {
  compose(style: StyleModifiers, input: string): void;
  getRouter(): ComposeDialogCallbackRouter;
}

export class ComposeApiProxyImpl implements ComposeApiProxy {
  static instance: ComposeApiProxy|null = null;

  handler: ComposeDialogPageHandlerInterface =
      new ComposeDialogPageHandlerRemote();
  composeDialogPageHandler = new ComposeDialogPageHandlerRemote();
  router = new ComposeDialogCallbackRouter();

  constructor() {
    const factoryRemote = ComposeDialogPageHandlerFactory.getRemote();
    factoryRemote.createComposeDialogPageHandler(
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
}
