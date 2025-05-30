// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PagesPerSheetSettingsElement} from './pages_per_sheet_settings.js';

export function getHtml(this: PagesPerSheetSettingsElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<print-preview-settings-section>
  <span id="pages-per-sheet-label" slot="title">$i18n{pagesPerSheetLabel}
  </span>
  <div slot="controls">
    <select class="md-select" aria-labelledby="pages-per-sheet-label"
        ?disabled="${this.disabled}" .value="${this.selectedValue}"
        @change="${this.onSelectChange}">
      <option value="1" selected>1</option>
      <option value="2">2</option>
      <option value="4">4</option>
      <option value="6">6</option>
      <option value="9">9</option>
      <option value="16">16</option>
    </select>
  </div>
</print-preview-settings-section>
<!--_html_template_end_-->`;
  // clang-format on
}
