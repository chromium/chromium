// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {getPrintPreviewPageHandler} from '../utils/mojo_data_providers.js';
import {type PrintPreviewPageHandler} from '../utils/print_preview_cros_app_types.js';

/**
 * @fileoverview
 * 'print_ticket_manager' responsible for tracking the active print ticket and
 * signaling updates to subscribed listeners.
 */

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

  // Prevent additional initialization.
  private constructor() {
    super();

    // Setup mojo data providers.
    this.printPreviewPageHandler = getPrintPreviewPageHandler();
  }

  // TODO(b/323421684): Takes current print ticket uses PrintPreviewPageHandler
  // to initiate actual print request. Also handles print request start and
  // finish events.
  sendPrintRequest(): void {
    assert(this.printPreviewPageHandler);

    // TODO(b/323421684): Handle result from page handler and update UI if error
    // occurred.
    this.printPreviewPageHandler!.print();
  }
}
