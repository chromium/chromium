// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CloseReason, ComposeClientPageHandlerRemote, ComposeDialogCallbackRouter, ComposeSessionPageHandlerFactory, ComposeSessionPageHandlerRemote, ComposeState, OpenMetadata, StyleModifiers, UserFeedback} from './compose.mojom-webui.js';

/** @interface */
export interface ComposeApiProxy {
  acceptComposeResult(): Promise<boolean>;
  closeUi(reason: CloseReason): void;
  compose(input: string): void;
  rewrite(style: StyleModifiers, input: string): void;
  getRouter(): ComposeDialogCallbackRouter;
  openBugReportingLink(): void;
  openComposeSettings(): void;
  setUserFeedback(reason: UserFeedback): void;
  requestInitialState(): Promise<OpenMetadata>;
  saveWebuiState(state: string): void;
  showUi(): void;
  undo(): Promise<(ComposeState | null)>;
}

export class ComposeApiProxyImpl implements ComposeApiProxy {
  static instance: ComposeApiProxy|null = null;

  composeSessionPageHandler = new ComposeSessionPageHandlerRemote();
  composeClientPageHandler = new ComposeClientPageHandlerRemote();
  router = new ComposeDialogCallbackRouter();

  constructor() {
    const factoryRemote = ComposeSessionPageHandlerFactory.getRemote();
    factoryRemote.createComposeSessionPageHandler(
        this.composeClientPageHandler.$.bindNewPipeAndPassReceiver(),
        this.composeSessionPageHandler.$.bindNewPipeAndPassReceiver(),
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
    return this.composeSessionPageHandler.acceptComposeResult().then(
        res => res.success);
  }

  closeUi(reason: CloseReason): void {
    this.composeClientPageHandler.closeUI(reason);
  }

  compose(input: string): void {
    this.composeSessionPageHandler.compose(input);
  }

  rewrite(style: StyleModifiers, input: string): void {
    this.composeSessionPageHandler.rewrite(style, input);
  }

  getRouter() {
    return this.router;
  }

  openBugReportingLink() {
    this.composeSessionPageHandler.openBugReportingLink();
  }

  openComposeSettings() {
    this.composeSessionPageHandler.openComposeSettings();
  }

  requestInitialState(): Promise<OpenMetadata> {
    return this.composeSessionPageHandler.requestInitialState().then(
        res => res.initialState);
  }

  saveWebuiState(state: string): void {
    this.composeSessionPageHandler.saveWebUIState(state);
  }

  showUi() {
    this.composeClientPageHandler.showUI();
  }

  setUserFeedback(reason: UserFeedback) {
    this.composeSessionPageHandler.setUserFeedback(reason);
  }

  undo(): Promise<(ComposeState | null)> {
    return this.composeSessionPageHandler.undo().then(
        composeState => composeState.lastState);
  }
}
