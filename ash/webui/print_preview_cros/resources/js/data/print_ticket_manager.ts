// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';

import {createCustomEvent} from '../utils/event_utils.js';
import {getPrintPreviewPageHandler} from '../utils/mojo_data_providers.js';
import {PrinterStatusReason, type PrintPreviewPageHandler, PrintTicket, SessionContext} from '../utils/print_preview_cros_app_types.js';

import {DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED, DestinationManager} from './destination_manager.js';
import {DEFAULT_PARTIAL_PRINT_TICKET} from './ticket_constants.js';

/**
 * @fileoverview
 * 'print_ticket_manager' responsible for tracking the active print ticket and
 * signaling updates to subscribed listeners.
 */

export const PRINT_REQUEST_STARTED_EVENT =
    'print-ticket-manager.print-request-started';
export const PRINT_REQUEST_FINISHED_EVENT =
    'print-ticket-manager.print-request-finished';
export const PRINT_TICKET_MANAGER_SESSION_INITIALIZED =
    'print-ticket-manager.session-initialized';

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
  private printTicket: PrintTicket|null = null;
  private sessionContext: SessionContext;
  private destinationManager: DestinationManager =
      DestinationManager.getInstance();
  private eventTracker = new EventTracker();

  // Prevent additional initialization.
  private constructor() {
    super();

    // Setup mojo data providers.
    this.printPreviewPageHandler = getPrintPreviewPageHandler();
  }

  // `initializeSession` is only intended to be called once from the
  // `PrintPreviewCrosAppController`.
  initializeSession(sessionContext: SessionContext): void {
    assert(
        !this.sessionContext, 'SessionContext should only be configured once');
    this.sessionContext = sessionContext;
    // TODO(b/323421684): Uses session context to configure ticket properties
    // and validating ticket matches policy requirements.
    this.printTicket = {
      // Set print ticket defaults.
      ...DEFAULT_PARTIAL_PRINT_TICKET,
      printPreviewId: this.sessionContext.printPreviewId,
      previewModifiable: this.sessionContext.isModifiable,
      shouldPrintSelectionOnly: this.sessionContext.hasSelection,
    } as PrintTicket;

    const activeDest = this.destinationManager.getActiveDestination();
    if (activeDest === null) {
      this.printTicket.destination = '';
      this.eventTracker.add(
          this.destinationManager,
          DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED,
          (event: Event): void => this.onActiveDestinationChanged(event));
    } else {
      this.printTicket.destination = activeDest.id;
      this.printTicket.printerType = activeDest.printerType;
      this.printTicket.printerManuallySelected =
          activeDest.printerManuallySelected;
      this.printTicket.printerStatusReason =
          activeDest.printerStatusReason || PrinterStatusReason.UNKNOWN_REASON;
    }

    // TODO(b/323421684): Apply default settings from destination capabilities
    // once capabilities manager has fetched active destination capabilities.

    this.dispatchEvent(
        createCustomEvent(PRINT_TICKET_MANAGER_SESSION_INITIALIZED));
  }

  // Handles notifying start and finish print request. Sends latest print ticket
  // state along with request.
  // TODO(b/323421684): Update print ticket prior to sending to set
  // headerFooterEnabled to false to align with Chrome preview behavior.
  sendPrintRequest(): void {
    assert(this.printPreviewPageHandler);

    if (this.printTicket === null) {
      // Print Ticket is not ready to be sent.
      return;
    }

    if (this.printRequestInProgress) {
      // Print is already in progress, wait for request to resolve before
      // allowing a second attempt.
      return;
    }

    this.printRequestInProgress = true;
    this.dispatchEvent(createCustomEvent(PRINT_REQUEST_STARTED_EVENT));

    // TODO(b/323421684): Handle result from page handler and update UI if error
    // occurred.
    this.printPreviewPageHandler!.print(this.printTicket).finally(() => {
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

  // Returns true only after the `initializeSession` function has been called
  // with a valid `SessionContext`.
  isSessionInitialized(): boolean {
    return !!this.sessionContext;
  }

  getPrintTicketForTesting(): PrintTicket|null {
    return this.printTicket;
  }

  // Handles setting initial active destination in print ticket if not already
  // set. Removes listener once destination is set in print ticket. After the
  // initial change, future updates to active destination will start in the
  // print ticket manager.
  private onActiveDestinationChanged(_event: Event): void {
    // Event listener added by initializeSession; print ticket will not be null.
    assert(this.printTicket);

    const activeDest = this.destinationManager.getActiveDestination();
    if (activeDest === null) {
      return;
    }

    if (this.printTicket!.destination === '') {
      this.printTicket!.destination = activeDest.id;
      this.printTicket!.printerType = activeDest.printerType;
      this.printTicket!.printerManuallySelected =
          activeDest.printerManuallySelected;
      this.printTicket!.printerStatusReason =
          activeDest.printerStatusReason || PrinterStatusReason.UNKNOWN_REASON;
    }

    this.eventTracker.remove(
        this.destinationManager,
        DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED);
  }
}

declare global {
  interface HTMLElementEventMap {
    [PRINT_REQUEST_FINISHED_EVENT]: CustomEvent<void>;
    [PRINT_REQUEST_STARTED_EVENT]: CustomEvent<void>;
  }
}
