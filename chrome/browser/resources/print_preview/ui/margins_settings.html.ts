// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {MarginsType} from '../data/margins.js';

import type {MarginsSettingsElement} from './margins_settings.js';

export function getHtml(this: MarginsSettingsElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<print-preview-settings-section>
  <span id="margins-label" slot="title">$i18n{marginsLabel}</span>
  <div slot="controls">
    <select class="md-select" aria-labelledby="margins-label"
        ?disabled="${this.marginsDisabled_}"
        .value="${this.selectedValue}" @change="${this.onSelectChange}">
      <!-- The order of these options must match the natural order of their
      values, which come from MarginsType. -->
      <option value="${MarginsType.DEFAULT}" selected>
        $i18n{defaultMargins}
      </option>
      <option value="${MarginsType.NO_MARGINS}">
        $i18n{noMargins}
      </option>
      <option value="${MarginsType.MINIMUM}">
        $i18n{minimumMargins}
      </option>
      <option value="${MarginsType.CUSTOM}">
        $i18n{customMargins}
      </option>
    </select>
  </div>
</print-preview-settings-section><!--_html_template_end_-->`;
  // clang-format on
}
