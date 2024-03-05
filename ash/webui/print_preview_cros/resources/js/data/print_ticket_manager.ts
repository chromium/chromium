// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {createCustomEvent} from '../utils/event_utils.js';
import {getPrintPreviewPageHandler} from '../utils/mojo_data_providers.js';
import {type PrintPreviewPageHandler} from '../utils/print_preview_cros_app_types.js';

/**
 * @fileoverview
 * 'print_ticket_manager' responsible for tracking the active print ticket and
 * signaling updates to subscribed listeners.
 */

export const PRINT_REQUEST_STARTED_EVENT =
    'print-ticket-manager.print-request-started';
export const PRINT_REQUEST_FINISHED_EVENT =
    'print-ticket-manager.print-request-finished';

export class PrintTicketManager extends EventTarget {
  private static instance: PrintTicketManager|null = null;

  static getInstance(): PrintTicketManager {
    if (PrintTicketManager.instance === null) {
      PrintTicketManager.instance = new PrintTicketManager();
    }

    return PrintTicketManager.instance;
  }

  static resetInstanceForTesting(): void {
    PrintTicketManager.instance = null;
  }

  // Non-static properties:
  private printPreviewPageHandler: PrintPreviewPageHandler|null;
  private printRequestInProgress = false;

  // Prevent additional initialization.
  private constructor() {
    super();

    // Setup mojo data providers.
    this.printPreviewPageHandler = getPrintPreviewPageHandler();
  }

  // Handles notifying start and finish print request.
  // TODO(b/323421684): Takes current print ticket uses PrintPreviewPageHandler
  // to initiate actual print request.
  sendPrintRequest(): void {
    assert(this.printPreviewPageHandler);

    if (this.printRequestInProgress) {
      // Print is already in progress, wait for request to resolve before
      // allowing a second attempt.
      return;
    }

    this.printRequestInProgress = true;
    this.dispatchEvent(createCustomEvent(PRINT_REQUEST_STARTED_EVENT));

    // TODO(b/323421684): Handle result from page handler and update UI if error
    // occurred.
    this.printPreviewPageHandler!.print().finally(() => {
      this.printRequestInProgress = false;
      this.dispatchEvent(createCustomEvent(PRINT_REQUEST_FINISHED_EVENT));
    });
  }

  // Does cleanup for print request.
  cancelPrintRequest(): void {
    assert(this.printPreviewPageHandler);
    this.printPreviewPageHandler!.cancel();
  }

  isPrintRequestInProgress(): boolean {
    return this.printRequestInProgress;
  }
}

declare global {
  interface HTMLElementEventMap {
    [PRINT_REQUEST_FINISHED_EVENT]: CustomEvent<void>;
    [PRINT_REQUEST_STARTED_EVENT]: CustomEvent<void>;
  }
}
