// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {DestinationSettingsElement} from './destination_settings.js';

export function getHtml(this: DestinationSettingsElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<print-preview-destination-select id="destinationSelect"
    ?dark="${this.dark}"
    .destination="${this.destination}"
    ?disabled="${this.shouldDisableDropdown_()}"
    ?loaded="${this.loaded_}"
    ?no-destinations="${this.noDestinations_}"
    ?pdf-printer-disabled="${this.pdfPrinterDisabled_}"
    .recentDestinationList="${this.displayedDestinations_}"
    @selected-option-change="${this.onSelectedDestinationOptionChange_}">
</print-preview-destination-select>
<cr-lazy-render-lit id="destinationDialog" .template="${() => html`
  <print-preview-destination-dialog
      .destinationStore="${this.destinationStore_}"
      @close="${this.onDialogClose_}">
  </print-preview-destination-dialog>
`}">
</cr-lazy-render-lit>
<!--_html_template_end_-->`;
  // clang-format on
}
