// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';
import {assert} from 'chrome://resources/js/assert.js';
import {UnguessableToken} from 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';

import {type PrintPreviewPageHandler, type PrintRequestOutcome, SessionContext} from '../utils/print_preview_cros_app_types.js';

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
  printPreviewId: new UnguessableToken(),
  isModifiable: true,
  hasSelection: true,
};

// Fake implementation of the PrintPreviewPageHandler for tests and UI.
export class FakePrintPreviewPageHandler implements PrintPreviewPageHandler {
  private methods: FakeMethodResolver = new FakeMethodResolver();
  private callCount: Map<string, number> = new Map<string, number>();
  private testDelayMs = 0;

  constructor() {
    this.registerMethods();
  }

  private registerMethods() {
    this.methods.register(PRINT_METHOD);
    this.methods.setResult(PRINT_METHOD, FAKE_PRINT_REQUEST_SUCCESSFUL);
    this.callCount.set(PRINT_METHOD, 0);
    this.methods.register(CANCEL_METHOD);
    this.callCount.set(CANCEL_METHOD, 0);
    this.methods.register(START_SESSION_METHOD);
    this.methods.setResult(
        START_SESSION_METHOD, FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL);
    this.callCount.set(START_SESSION_METHOD, 0);
  }

  // Handles restoring state of fake to initial state.
  reset(): void {
    this.callCount.clear();
    this.methods = new FakeMethodResolver();
    this.registerMethods();
    this.testDelayMs = 0;
  }

  setPrintResult(result: PrintRequestOutcome) {
    this.methods.setResult(PRINT_METHOD, result);
  }

  // Incrementing call count of tracked method.
  private incrementCallCount(methodName: string): void {
    const prevCallCount = this.callCount.get(methodName) ?? 0;
    this.callCount.set(methodName, prevCallCount + 1);
  }

  // Mock implementation of print.
  print(): Promise<PrintRequestOutcome> {
    this.incrementCallCount(PRINT_METHOD);
    return this.methods.resolveMethodWithDelay(PRINT_METHOD, this.testDelayMs);
  }

  // Mock implementation of startSession.
  startSession(): Promise<SessionContext> {
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

  setTestDelay(delay: number): void {
    assert(delay >= 0);
    this.testDelayMs = delay;
  }

  getMethodsForTesting(): FakeMethodResolver {
    return this.methods;
  }
}
