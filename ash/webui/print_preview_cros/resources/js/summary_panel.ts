// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../css/print_preview_cros_shared.css.js';
import 'chrome://resources/cros_components/button/button.js';

import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './summary_panel.html.js';
import {PRINT_BUTTON_DISABLED_CHANGED_EVENT, SHEETS_USED_CHANGED_EVENT, SummaryPanelController} from './summary_panel_controller.js';

/**
 * @fileoverview
 * 'summary-panel' manages the print and cancel functionality as well as
 * displays the current count of sheets used;
 */

export class SummaryPanelElement extends PolymerElement {
  static get is() {
    return 'summary-panel' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      sheetsUsedText: String,
      printButtonDisabled: Boolean,
    };
  }

  private controller: SummaryPanelController;
  private eventTracker = new EventTracker();
  private sheetsUsedText: string;
  private printButtonDisabled: boolean;

  override connectedCallback(): void {
    super.connectedCallback();

    this.controller = new SummaryPanelController(this.eventTracker);
    this.eventTracker.add(
        this.controller, PRINT_BUTTON_DISABLED_CHANGED_EVENT,
        () => this.onPrintButtonDisabledChanged());
    this.eventTracker.add(
        this.controller, SHEETS_USED_CHANGED_EVENT,
        () => this.onSheetsUsedChanged());

    // Initialize properties using controller.
    this.sheetsUsedText = this.controller.getSheetsUsedText();
    this.printButtonDisabled = this.controller.shouldDisablePrintButton();
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    this.eventTracker.removeAll();
  }

  getControllerForTesting(): SummaryPanelController {
    return this.controller;
  }

  private onSheetsUsedChanged(): void {
    this.sheetsUsedText = this.controller.getSheetsUsedText();
  }

  // Click event handler for `#print` button.
  protected onPrintClicked(): void {
    this.controller.handlePrintClicked();
  }

  // Click event handler for `#cancel` button.
  protected onCancelClicked(): void {
    this.controller.handleCancelClicked();
  }

  // Ensure `#print` disabled state updates based on controller's
  // shouldDisablePrintButton method on `PRINT_BUTTON_DISABLED_CHANGED_EVENT`.
  private onPrintButtonDisabledChanged(): void {
    this.printButtonDisabled = this.controller.shouldDisablePrintButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SummaryPanelElement.is]: SummaryPanelElement;
  }
}

customElements.define(SummaryPanelElement.is, SummaryPanelElement);
