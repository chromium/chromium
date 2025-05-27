// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CopiesSettingsElement} from './copies_settings.js';

export function getHtml(this: CopiesSettingsElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<print-preview-number-settings-section max-value="${this.copiesMax_}"
    min-value="1" default-value="1" input-label="$i18n{copiesLabel}"
    input-aria-label="$i18n{copiesLabel}"
    ?disabled="${this.disabled}" current-value="${this.currentValue_}"
    @current-value-changed="${this.onCurrentValueChanged_}"
    ?input-valid="${this.inputValid_}"
    @input-valid-changed="${this.onInputValidChanged_}"
    hint-message="${this.getHintMessage_()}">
  <div slot="opt-inside-content" class="checkbox" aria-live="polite"
      ?hidden="${this.collateHidden_()}">
    <cr-checkbox id="collate" @change="${this.onCollateChange_}"
        ?disabled="${this.disabled}" aria-labelledby="copies-collate-label">
      <span id="copies-collate-label">$i18n{optionCollate}</span>
    </cr-checkbox>
  </div>
</print-preview-number-settings-section>
<!--_html_template_end_-->`;
  // clang-format on
}
