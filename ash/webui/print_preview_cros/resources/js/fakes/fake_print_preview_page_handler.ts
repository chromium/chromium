// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';

import {type PrintPreviewPageHandler, type PrintRequestOutcome} from '../utils/print_preview_cros_app_types.js';

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

// Fake implementation of the PrintPreviewPageHandler for tests and UI.
export class FakePrintPreviewPageHandler implements PrintPreviewPageHandler {
  private methods: FakeMethodResolver = new FakeMethodResolver();
  private callCount: Map<string, number> = new Map<string, number>();
  constructor() {
    this.registerMethods();
  }

  private registerMethods() {
    this.methods.register(PRINT_METHOD);
    this.methods.setResult(PRINT_METHOD, FAKE_PRINT_REQUEST_SUCCESSFUL);
    this.callCount.set(PRINT_METHOD, 0);
  }

  // Handles restoring state of fake to initial state.
  reset(): void {
    this.callCount.clear();
    this.methods = new FakeMethodResolver();
    this.registerMethods();
  }

  setPrintResult(result: PrintRequestOutcome) {
    this.methods.setResult(PRINT_METHOD, result);
  }

  // Mock implementation of print.
  print(): Promise<PrintRequestOutcome> {
    const prevCallCount = this.callCount.get(PRINT_METHOD) ?? 0;
    this.callCount.set(PRINT_METHOD, prevCallCount + 1);
    return this.methods.resolveMethod(PRINT_METHOD);
  }

  getCallCount(method: string): number {
    return this.callCount.get(method) ?? 0;
  }
}
