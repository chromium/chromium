// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AdvancedSettingsItemElement} from './advanced_settings_item.js';

export function getHtml(this: AdvancedSettingsItemElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<label class="label searchable">${this.getDisplayName_(this.capability)}</label>
<div class="value">
  ${this.isCapabilityTypeSelect_() ? html`
    <div>
      <select class="md-select" @change="${this.onUserInput_}">
        ${this.capability.select_cap!.option!.map(item => html`
          <option class="searchable" value="${item.value}"
              ?selected="${this.isOptionSelected_(item)}">
             ${this.getDisplayName_(item)}
          </option>
        `)}
      </select>
    </div>
  ` : ''}
  <span ?hidden="${!this.isCapabilityTypeInput_()}">
    <cr-input type="text" @input="${this.onUserInput_}" spellcheck="false"
        placeholder="${this.getCapabilityPlaceholder_()}">
    </cr-input>
  </span>
  <span ?hidden="${!this.isCapabilityTypeCheckbox_()}">
    <cr-checkbox @change="${this.onCheckboxInput_}"
        ?checked="${this.isChecked_()}">
    </cr-checkbox>
  </span>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
