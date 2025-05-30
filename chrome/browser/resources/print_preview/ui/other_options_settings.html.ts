// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OtherOptionsSettingsElement} from './other_options_settings.js';

export function getHtml(this: OtherOptionsSettingsElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${this.options_.map((item, index) => html`
  <print-preview-settings-section ?hidden="${!item.available}"
      class="${this.getClass_(index)}">
    <div slot="title">
      <span class="title">$i18n{optionsLabel}</span>
    </div>
    <div slot="controls" class="checkbox">
      <cr-checkbox id="${item.name}" data-index="${index}"
          ?disabled="${this.getDisabled_(item.managed)}"
          @change="${this.onChange_}" ?checked="${item.value}">
        <span>${this.i18n(item.label)}</span>
      </cr-checkbox>
    </div>
  </print-preview-settings-section>
`)}
<!--_html_template_end_-->`;
  // clang-format on
}
