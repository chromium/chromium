// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsSelectElement} from './settings_select.js';

export function getHtml(this: SettingsSelectElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<select class="md-select" ?disabled="${this.disabled}"
    aria-label="${this.ariaLabel}" .value="${this.selectedValue}"
    @change="${this.onSelectChange}">
  ${this.capability ? html`
    ${this.capability.option.map(item => html`
      <option ?selected="${this.isSelected_(item)}"
          value="${this.getValue_(item)}">
        ${this.getDisplayName_(item)}
      </option>
    `)}
  ` : ''}
</select>
<!--_html_template_end_-->`;
  // clang-format on
}
