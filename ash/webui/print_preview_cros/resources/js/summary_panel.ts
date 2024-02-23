// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cros_components/button/button.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './summary_panel.html.js';
import {SHEETS_USED_CHANGED_EVENT, SummaryPanelController} from './summary_panel_controller.js';

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
    };
  }

  private controller: SummaryPanelController = new SummaryPanelController();
  private sheetsUsedText: string;

  override connectedCallback(): void {
    super.connectedCallback();
    this.controller.addEventListener(
        SHEETS_USED_CHANGED_EVENT, (e: Event) => this.onSheetsUsedChanged(e));

    // Initialize properties using controller.
    this.sheetsUsedText = this.controller.getSheetsUsedText();
  }

  getControllerForTesting() {
    return this.controller;
  }

  private onSheetsUsedChanged(_event: Event) {
    this.sheetsUsedText = this.controller.getSheetsUsedText();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SummaryPanelElement.is]: SummaryPanelElement;
  }
}

customElements.define(SummaryPanelElement.is, SummaryPanelElement);
