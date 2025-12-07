// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ColorSettingsElement} from './color_settings.js';

export function getHtml(this: ColorSettingsElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<print-preview-settings-section>
  <span id="color-label" slot="title">$i18n{optionColor}</span>
  <div slot="controls">
    <select class="md-select" aria-labelledby="color-label"
        ?disabled="${this.computeDisabled_()}" .value="${this.selectedValue}"
        @change="${this.onSelectChange}">
      <option value="bw" selected>$i18n{optionBw}</option>
      <option value="color">$i18n{optionColor}</option>
    </select>
  </div>
</print-preview-settings-section>
<!--_html_template_end_-->`;
  // clang-format on
}
