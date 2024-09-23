// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';
import {FakeObservables} from 'chrome://resources/ash/common/fake_observables.js';
import {assert} from 'chrome://resources/js/assert.js';
import {UnguessableToken} from 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';

import {FakeGeneratePreviewObserver, PreviewTicket, type PrintPreviewPageHandler, type PrintRequestOutcome, SessionContext} from '../utils/print_preview_cros_app_types.js';
import type {PrintTicket} from '../utils/print_preview_cros_app_types.js';

/**
 * @fileoverview
 * 'fake_print_preview_page_handler' is a mock implementation of the
 * `PrintPreviewPageHandler` mojo interface.
 */

const PRINT_METHOD = 'print';
export const FAKE_PRINT_REQUEST_SUCCESSFUL: PrintRequestOutcome = {
  success: true,
};

export const FAKE_PRINT_REQUEST_FAILURE_INVALID_SETTINGS_ERROR:
    PrintRequestOutcome = {
      success: false,
      error: 'Invalid settings',
    };

const CANCEL_METHOD = 'cancel';
const START_SESSION_METHOD = 'startSession';
export const FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL: SessionContext = {
  printPreviewToken: new UnguessableToken(),
  isModifiable: true,
  hasSelection: true,
};

const GENERATE_PREVIEW_METHOD = 'generatePreview';

export const OBSERVE_PREVIEW_READY_METHOD = 'observePreviewReady';
const OBSERVABLE_ON_DOCUMENT_READY = 'onDocumentReady';

// Fake implementation of the PrintPreviewPageHandler for tests and UI.
export class FakePrintPreviewPageHandler implements PrintPreviewPageHandler {
  private methods: FakeMethodResolver = new FakeMethodResolver();
  private callCount: Map<string, number> = new Map<string, number>();
  private testDelayMs = 0;
  private observables: FakeObservables = new FakeObservables();
  private previewTicket: PreviewTicket|null = null;
  dialogArgs = '';

  constructor() {
    this.registerMethods();
    this.registerObservables();
  }

  private registerMethods() {
    this.methods.register(PRINT_METHOD);
    this.methods.setResult(
        PRINT_METHOD, {printRequestOutcome: FAKE_PRINT_REQUEST_SUCCESSFUL});
    this.callCount.set(PRINT_METHOD, 0);
    this.methods.register(CANCEL_METHOD);
    this.callCount.set(CANCEL_METHOD, 0);
    this.methods.register(START_SESSION_METHOD);
    this.methods.setResult(
        START_SESSION_METHOD,
        {sessionContext: FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL});
    this.callCount.set(START_SESSION_METHOD, 0);
    this.methods.register(GENERATE_PREVIEW_METHOD);
    this.callCount.set(GENERATE_PREVIEW_METHOD, 0);
    this.methods.register(OBSERVE_PREVIEW_READY_METHOD);
    this.callCount.set(OBSERVE_PREVIEW_READY_METHOD, 0);
  }

  private registerObservables(): void {
    this.observables.register(OBSERVABLE_ON_DOCUMENT_READY);
  }

  // Handles restoring state of fake to initial state.
  reset(): void {
    this.callCount.clear();
    this.methods = new FakeMethodResolver();
    this.registerMethods();
    this.testDelayMs = 0;
    this.previewTicket = null;
    this.dialogArgs = '';
  }

  setPrintResult(result: PrintRequestOutcome) {
    this.methods.setResult(PRINT_METHOD, {printRequestOutcome: result});
  }

  // Incrementing call count of tracked method.
  private incrementCallCount(methodName: string): void {
    const prevCallCount = this.callCount.get(methodName) ?? 0;
    this.callCount.set(methodName, prevCallCount + 1);
  }

  // Mock implementation of print.
  print(_ticket: PrintTicket):
      Promise<{printRequestOutcome: PrintRequestOutcome}> {
    this.incrementCallCount(PRINT_METHOD);
    return this.methods.resolveMethodWithDelay(PRINT_METHOD, this.testDelayMs);
  }

  // Mock implementation of startSession.
  startSession(dialogArgs: string): Promise<{sessionContext: SessionContext}> {
    this.dialogArgs = dialogArgs;
    this.incrementCallCount(START_SESSION_METHOD);
    return this.methods.resolveMethodWithDelay(
        START_SESSION_METHOD, this.testDelayMs);
  }

  getCallCount(method: string): number {
    return this.callCount.get(method) ?? 0;
  }

  // Mock implementation of cancel.
  cancel(): void {
    this.incrementCallCount(CANCEL_METHOD);
  }

  generatePreview(previewTicket: PreviewTicket): Promise<void> {
    this.incrementCallCount(GENERATE_PREVIEW_METHOD);
    this.previewTicket = previewTicket;
    return this.methods.resolveMethodWithDelay(
        GENERATE_PREVIEW_METHOD, this.testDelayMs);
  }

  observePreviewReady(observer: FakeGeneratePreviewObserver): Promise<void> {
    this.observables.observe(
        OBSERVABLE_ON_DOCUMENT_READY, (previewRequestId: number): void => {
          observer.onDocumentReady(previewRequestId);
        });
    this.incrementCallCount(OBSERVE_PREVIEW_READY_METHOD);
    return this.methods.resolveMethodWithDelay(
        OBSERVE_PREVIEW_READY_METHOD, this.testDelayMs);
  }

  triggerOnDocumentReady(previewRequestId: number) {
    this.observables.setObservableData(
        OBSERVABLE_ON_DOCUMENT_READY, [previewRequestId]);
    this.observables.trigger(OBSERVABLE_ON_DOCUMENT_READY);
  }

  triggerOnDocumentReadyActiveRequestId() {
    assert(this.previewTicket);
    this.triggerOnDocumentReady(this.previewTicket.requestId);
  }

  getPreviewTicket(): PreviewTicket|null {
    return this.previewTicket;
  }

  setTestDelay(delay: number): void {
    assert(delay >= 0);
    this.testDelayMs = delay;
  }

  getMethodsForTesting(): FakeMethodResolver {
    return this.methods;
  }
}
