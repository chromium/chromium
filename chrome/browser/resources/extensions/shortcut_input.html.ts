// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ShortcutInputElement} from './shortcut_input.js';

export function getHtml(this: ShortcutInputElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="main">
  <cr-input id="input" ?readonly="${this.readonly_}"
      aria-label="${this.computeInputAriaLabel_()}"
      .placeholder="${this.computePlaceholder_()}"
      ?invalid="${this.getIsInvalid_()}"
      .errorMessage="${this.getErrorString_()}"
      .value="${this.computeText_()}">
    <cr-icon-button id="edit" title="$i18n{edit}"
        aria-label="${this.computeEditButtonAriaLabel_()}"
        slot="suffix" class="icon-edit no-overlap"
        @click="${this.onEditClick_}">
    </cr-icon-button>
  </cr-input>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
