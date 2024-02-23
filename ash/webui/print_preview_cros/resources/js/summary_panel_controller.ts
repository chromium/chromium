// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

/**
 * @fileoverview
 * 'summary-panel-controller' defines events and event handlers to correctly
 * consume changes from mojo providers and inform the `summary-panel` element
 * to update.
 */

export const SHEETS_USED_CHANGED_EVENT =
    'summary-panel-controller.sheets-used-changed';

// SummaryPanelController defines functionality used to update the
// `summary-panel` element.
export class SummaryPanelController extends EventTarget {
  private sheetsUsed = 0;

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
    this.dispatchEvent(new CustomEvent<void>(
        SHEETS_USED_CHANGED_EVENT, {bubbles: true, composed: true}));
  }
}

declare global {
  interface HTMLElementEventMap {
    [SHEETS_USED_CHANGED_EVENT]: CustomEvent<void>;
  }
}
