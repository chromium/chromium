// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {ScalingType} from '../data/scaling.js';

import type {ScalingSettingsElement} from './scaling_settings.js';

export function getHtml(this: ScalingSettingsElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<print-preview-settings-section>
  <span slot="title" id="scaling-label">$i18n{scalingLabel}</span>
  <div slot="controls">
    <select class="md-select" aria-labelledby="scaling-label"
        ?disabled="${this.dropdownDisabled_}" .value="${this.selectedValue}"
        @change="${this.onSelectChange}">
      <option value="${ScalingType.DEFAULT}"
          ?selected="${this.isSelected_(ScalingType.DEFAULT)}">
        $i18n{optionDefaultScaling}
      </option>
      <option value="${ScalingType.FIT_TO_PAGE}" ?hidden="${!this.isPdf}"
          ?disabled="${!this.isPdf}"
          ?selected="${this.isSelected_(ScalingType.FIT_TO_PAGE)}">
        $i18n{optionFitToPage}
      </option>
      <option value="${ScalingType.FIT_TO_PAPER}" ?hidden="${!this.isPdf}"
          ?disabled="${!this.isPdf}"
          ?selected="${this.isSelected_(ScalingType.FIT_TO_PAPER)}">
        $i18n{optionFitToPaper}
      </option>
      <option value="${ScalingType.CUSTOM}"
          ?selected="${this.isSelected_(ScalingType.CUSTOM)}">
        $i18n{optionCustomScaling}
      </option>
    </select>
  </div>
</print-preview-settings-section>
<cr-collapse ?opened="${this.customSelected_}"
    @transitionend="${this.onCollapseChanged_}">
  <print-preview-number-settings-section
      max-value="200" min-value="10" default-value="100"
      ?disabled="${this.inputDisabled_()}"
      current-value="${this.currentValue_}"
      @current-value-changed="${this.onCurrentValueChanged_}"
      ?input-valid="${this.inputValid_}"
      @input-valid-changed="${this.onInputValidChanged_}"
      hint-message="$i18n{scalingInstruction}">
  </print-preview-number-settings-section>
</cr-collapse><!--_html_template_end_-->`;
  // clang-format on
}
