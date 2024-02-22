// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cros_components/button/button.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './summary_panel.html.js';

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
}

declare global {
  interface HTMLElementTagNameMap {
    [SummaryPanelElement.is]: SummaryPanelElement;
  }
}

customElements.define(SummaryPanelElement.is, SummaryPanelElement);
