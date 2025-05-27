// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {LayoutSettingsElement} from './layout_settings.js';

export function getHtml(this: LayoutSettingsElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<print-preview-settings-section>
  <span id="layout-label" slot="title">$i18n{layoutLabel}</span>
  <div slot="controls">
    <select class="md-select" aria-labelledby="layout-label"
        ?disabled="${this.disabled}" .value="${this.selectedValue}"
        @change="${this.onSelectChange}">
      <option value="portrait" selected>$i18n{optionPortrait}</option>
      <option value="landscape">$i18n{optionLandscape}</option>
    </select>
  </div>
</print-preview-settings-section>
<!--_html_template_end_-->`;
  // clang-format on
}
