// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';

import {PRINT_REQUEST_FINISHED_EVENT, PRINT_REQUEST_STARTED_EVENT, PrintTicketManager} from './data/print_ticket_manager.js';

/**
 * @fileoverview
 * 'summary-panel-controller' defines events and event handlers to correctly
 * consume changes from mojo providers and inform the `summary-panel` element
 * to update.
 */

export const PRINT_BUTTON_DISABLED_CHANGED_EVENT =
    'summary-panel-controller.print-button-disabled-changed';
export const SHEETS_USED_CHANGED_EVENT =
    'summary-panel-controller.sheets-used-changed';

// SummaryPanelController defines functionality used to update the
// `summary-panel` element.
export class SummaryPanelController extends EventTarget {
  private sheetsUsed = 0;
  private printTicketManger = PrintTicketManager.getInstance();

  /**
   * @param eventTracker Passed in by owning element to ensure event handlers
   * lifetime is aligned with element.
   */
  constructor(eventTracker: EventTracker) {
    super();
    eventTracker.add(
        this.printTicketManger, PRINT_REQUEST_STARTED_EVENT,
        (e: Event) => this.onPrintRequestStarted(e));
    eventTracker.add(
        this.printTicketManger, PRINT_REQUEST_FINISHED_EVENT,
        (e: Event) => this.onPrintRequestFinished(e));
  }

  // Returns localized string based on current number of sheets in document and
  // whether document is being saved to a digital destination or printed to a
  // physical location.
  // TODO(b/323421684, b/323585997): Use localized string to correctly display
  // count of sheets used or pages depending on print settings and destination.
  getSheetsUsedText(): string {
    if (this.sheetsUsed <= 0) {
      return '';
    }

    return `${this.sheetsUsed} used`;
  }

  setSheetsUsedForTesting(sheetsUsed: number): void {
    assert(sheetsUsed >= 0);
    this.sheetsUsed = sheetsUsed;
    this.dispatch(SHEETS_USED_CHANGED_EVENT);
  }

  // Handles behavior for when the print button is clicked.
  handlePrintClicked(): void {
    this.printTicketManger.sendPrintRequest();
  }

  // Handles any required cleanup prior to sending a cancel request to the
  // backend and closing the dialog when the cancel button is clicked.
  handleCancelClicked(): void {
    this.printTicketManger.cancelPrintRequest();
  }

  // CustomEvent dispatch helper.
  private dispatch(eventName: string): void {
    this.dispatchEvent(
        new CustomEvent<void>(eventName, {bubbles: true, composed: true}));
  }

  // Handles notifying UI to update state when print request starts.
  private onPrintRequestStarted(_e: Event): void {
    this.dispatch(PRINT_BUTTON_DISABLED_CHANGED_EVENT);
  }

  // Handles notifying UI to update state when print request finishes.
  private onPrintRequestFinished(_e: Event): void {
    this.dispatch(PRINT_BUTTON_DISABLED_CHANGED_EVENT);
  }

  // Whether the print button should be enabled for the current state.
  shouldDisablePrintButton(): boolean {
    return this.printTicketManger.isPrintRequestInProgress();
  }
}

declare global {
  interface HTMLElementEventMap {
    [SHEETS_USED_CHANGED_EVENT]: CustomEvent<void>;
  }
}
