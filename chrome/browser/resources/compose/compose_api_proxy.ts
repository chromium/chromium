// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CloseReason, ComposeState, InputMode, OpenMetadata, StyleModifier, UserFeedback} from './compose.mojom-webui.js';
import {ComposeClientUntrustedPageHandlerRemote, ComposeSessionUntrustedPageHandlerFactory, ComposeSessionUntrustedPageHandlerRemote, ComposeUntrustedDialogCallbackRouter} from './compose.mojom-webui.js';

/** @interface */
export interface ComposeApiProxy {
  acceptComposeResult(): Promise<boolean>;
  logCancelEdit(): void;
  completeFirstRun(): void;
  closeUi(reason: CloseReason): void;
  compose(input: string, mode: InputMode, edited: boolean): void;
  rewrite(style: StyleModifier | null): void;
  logEditInput(): void;
  getRouter(): ComposeUntrustedDialogCallbackRouter;
  openBugReportingLink(): void;
  openComposeLearnMorePage(): void;
  openComposeSettings(): void;
  openFeedbackSurveyLink(): void;
  openSignInPage(): void;
  setUserFeedback(reason: UserFeedback): void;
  requestInitialState(): Promise<OpenMetadata>;
  saveWebuiState(state: string): void;
  showUi(): void;
  recoverFromErrorState(): Promise<(ComposeState | null)>;
  undo(): Promise<(ComposeState | null)>;
  redo(): Promise<(ComposeState | null)>;
  editResult(newText: string): Promise<boolean>;
}

export class ComposeApiProxyImpl implements ComposeApiProxy {
  static instance: ComposeApiProxy|null = null;

  composeSessionPageHandler = new ComposeSessionUntrustedPageHandlerRemote();
  composeClientPageHandler = new ComposeClientUntrustedPageHandlerRemote();
  router = new ComposeUntrustedDialogCallbackRouter();

  constructor() {
    const factoryRemote = ComposeSessionUntrustedPageHandlerFactory.getRemote();
    factoryRemote.createComposeSessionUntrustedPageHandler(
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

  logCancelEdit(): void {
    this.composeSessionPageHandler.logCancelEdit();
  }

  completeFirstRun(): void {
    this.composeClientPageHandler.completeFirstRun();
  }

  closeUi(reason: CloseReason): void {
    this.composeClientPageHandler.closeUI(reason);
  }

  openComposeSettings() {
    this.composeClientPageHandler.openComposeSettings();
  }

  compose(input: string, mode: InputMode, edited: boolean): void {
    this.composeSessionPageHandler.compose(input, mode, edited);
  }

  rewrite(style: StyleModifier): void {
    this.composeSessionPageHandler.rewrite(style);
  }

  logEditInput(): void {
    this.composeSessionPageHandler.logEditInput();
  }

  getRouter() {
    return this.router;
  }

  openBugReportingLink() {
    this.composeSessionPageHandler.openBugReportingLink();
  }

  openComposeLearnMorePage() {
    this.composeSessionPageHandler.openComposeLearnMorePage();
  }

  openFeedbackSurveyLink() {
    this.composeSessionPageHandler.openFeedbackSurveyLink();
  }

  openSignInPage() {
    this.composeSessionPageHandler.openSignInPage();
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

  recoverFromErrorState(): Promise<(ComposeState | null)> {
    return this.composeSessionPageHandler.recoverFromErrorState().then(
        composeState => composeState.stateBeforeError);
  }

  editResult(newResult: string): Promise<boolean> {
    return this.composeSessionPageHandler.editResult(newResult).then(
        res => res.isEdited);
  }

  redo(): Promise<(ComposeState | null)> {
    return this.composeSessionPageHandler.redo().then(
        composeState => composeState.nextState);
  }
}
