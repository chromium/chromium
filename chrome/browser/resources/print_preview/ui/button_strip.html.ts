// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ButtonStripElement} from './button_strip.js';

export function getHtml(this: ButtonStripElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<if expr="not is_win">
  <cr-button class="cancel-button" @click="${this.onCancelClick_}">
    $i18n{cancel}
  </cr-button>
</if>
  <cr-button class="action-button" @click="${this.onPrintClick_}"
      ?disabled="${!this.printButtonEnabled_}">
    ${this.printButtonLabel_}
  </cr-button>
<if expr="is_win">
  <cr-button class="cancel-button" @click="${this.onCancelClick_}">
    $i18n{cancel}
  </cr-button>
</if>
<!--_html_template_end_-->`;
  // clang-format on
}
