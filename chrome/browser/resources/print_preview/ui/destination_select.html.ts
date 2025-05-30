// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {DestinationSelectElement} from './destination_select.js';

export function getHtml(this: DestinationSelectElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<print-preview-settings-section>
  <span id="destination-label" slot="title">$i18n{destinationLabel}</span>
  <div slot="controls">
    <div class="throbber-container" ?hidden="${this.loaded}">
      <div class="throbber"></div>
    </div>
    <select class="md-select" aria-labelledby="destination-label"
        ?hidden="${!this.loaded}" ?disabled="${this.disabled}"
        .style="background-image:${this.getBackgroundImages_()};"
        @change="${this.onSelectChange}">
      ${this.recentDestinationList.map(item => html`
        <option value="${item.key}" ?selected="${this.isSelected_(item.key)}">
          ${item.displayName}
        </option>
      `)}
      <option value="${this.pdfDestinationKey_}"
          ?hidden="${this.pdfPrinterDisabled}"
          ?selected="${this.isSelected_(this.pdfDestinationKey_)}">
        $i18n{printToPDF}
      </option>
      <option value="noDestinations" ?hidden="${!this.noDestinations}"
          ?selected="${this.noDestinations}">
        $i18n{noDestinationsMessage}
      </option>
      <option value="seeMore" aria-label="$i18n{seeMoreDestinationsLabel}">
        $i18n{seeMore}
      </option>
    </select>
  </div>
</print-preview-settings-section>
<!--_html_template_end_-->`;
  // clang-format on
}
