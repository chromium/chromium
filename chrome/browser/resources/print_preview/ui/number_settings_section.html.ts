// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {NumberSettingsSectionElement} from './number_settings_section.js';

export function getHtml(this: NumberSettingsSectionElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<print-preview-settings-section>
  <span slot="title" id="sectionTitle">${this.inputLabel}</span>
  <div slot="controls" id="controls">
    <span class="input-wrapper">
      <cr-input id="userValue" type="number" class="stroked"
          max="${this.maxValue}" min="${this.minValue}" data-timeout-delay="250"
          ?disabled="${this.getDisabled_()}" @keydown="${this.onKeydown_}"
          @blur="${this.onBlur_}" aria-label="${this.inputAriaLabel}"
          error-message="${this.errorMessage_}" auto-validate>
        <span slot="suffix">
          <slot name="opt-inside-content"></slot>
        </span>
      </cr-input>
    </span>
  </div>
</print-preview-settings-section>
<!--_html_template_end_-->`;
  // clang-format on
}
