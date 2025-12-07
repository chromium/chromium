// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ExtensionsToggleRowElement} from './toggle_row.js';

export function getHtml(this: ExtensionsToggleRowElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<label id="label" aria-hidden="true">
  <input id="native" type="checkbox" .checked="${this.checked}"
      @change="${this.onNativeChange_}" @click="${this.onNativeClick_}"
      .disabled="${this.disabled}">
  <slot></slot>
</label>
<cr-toggle id="crToggle" ?checked="${this.checked}" aria-labelledby="label"
    @change="${this.onCrToggleChange_}" ?disabled="${this.disabled}">
</cr-toggle>
<!--_html_template_end_-->`;
  // clang-format on
}
