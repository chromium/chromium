// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ComposeDialogCallbackRouter, ComposeDialogPageHandlerFactory, ComposeDialogPageHandlerInterface, ComposeDialogPageHandlerRemote, ComposeResponse, StyleModifiers} from './compose.mojom-webui.js';

/** @interface */
export interface ComposeApiProxy {
  compose(style: StyleModifiers, input: string): Promise<ComposeResponse>;
}

export class ComposeApiProxyImpl implements ComposeApiProxy {
  static instance: ComposeApiProxy|null = null;

  handler: ComposeDialogPageHandlerInterface =
      new ComposeDialogPageHandlerRemote();
  composeDialogPageHandler = new ComposeDialogPageHandlerRemote();
  composeDialogCallbackRouter = new ComposeDialogCallbackRouter();

  constructor() {
    const factoryRemote = ComposeDialogPageHandlerFactory.getRemote();
    factoryRemote.createComposeDialogPageHandler(
        this.composeDialogPageHandler.$.bindNewPipeAndPassReceiver(),
        this.composeDialogCallbackRouter.$.bindNewPipeAndPassRemote());
  }

  static getInstance(): ComposeApiProxy {
    return ComposeApiProxyImpl.instance ||
        (ComposeApiProxyImpl.instance = new ComposeApiProxyImpl());
  }

  static setInstance(newInstance: ComposeApiProxy) {
    ComposeApiProxyImpl.instance = newInstance;
  }

  /** @override */
  compose(style: StyleModifiers, input: string): Promise<ComposeResponse> {
    return this.composeDialogPageHandler.compose(style, input)
        .then(composeResponse => composeResponse.response);
  }
}
