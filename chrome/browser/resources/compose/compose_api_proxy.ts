// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CloseReason, ComposeDialogCallbackRouter, ComposeDialogClosePageHandlerRemote, ComposeDialogPageHandlerFactory, ComposeDialogPageHandlerRemote, ComposeState, OpenMetadata, StyleModifiers} from './compose.mojom-webui.js';

/** @interface */
export interface ComposeApiProxy {
  acceptComposeResult(): Promise<boolean>;
  closeUi(reason: CloseReason): void;
  compose(style: StyleModifiers, input: string): void;
  getRouter(): ComposeDialogCallbackRouter;
  openBugReportingLink(): void;
  requestInitialState(): Promise<OpenMetadata>;
  saveWebuiState(state: string): void;
  undo(): Promise<(ComposeState | null)>;
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

  acceptComposeResult(): Promise<boolean> {
    return this.composeDialogPageHandler.acceptComposeResult().then(
        res => res.success);
  }

  closeUi(reason: CloseReason): void {
    this.composeDialogClosePageHandler.closeUI(reason);
  }

  compose(style: StyleModifiers, input: string): void {
    this.composeDialogPageHandler.compose(style, input);
  }

  getRouter() {
    return this.router;
  }

  openBugReportingLink() {
    this.composeDialogPageHandler.openBugReportingLink();
  }

  requestInitialState(): Promise<OpenMetadata> {
    return this.composeDialogPageHandler.requestInitialState().then(
        res => res.initialState);
  }

  saveWebuiState(state: string): void {
    this.composeDialogPageHandler.saveWebUIState(state);
  }

  undo(): Promise<(ComposeState | null)> {
    return this.composeDialogPageHandler.undo().then(
        composeState => composeState.lastState);
  }
}
