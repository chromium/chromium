// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview
 * 'print_ticket_manager' responsible for tracking the active print ticket and
 * signaling updates to subscribed listeners.
 */

export class PrintTicketManager extends EventTarget {
  private static instance: PrintTicketManager|null = null;

  // Prevent additional initialization.
  private constructor() {
    super();
  }

  static getInstance(): PrintTicketManager {
    if (PrintTicketManager.instance === null) {
      PrintTicketManager.instance = new PrintTicketManager();
    }

    return PrintTicketManager.instance;
  }

  static resetInstanceForTesting(): void {
    PrintTicketManager.instance = null;
  }

  // TODO(b/323421684): Takes current print ticket uses PrintPreviewPageHandler
  // to initiate actual print request. Also handles print request start and
  // finish events.
  sendPrintRequest(): void {}
}
