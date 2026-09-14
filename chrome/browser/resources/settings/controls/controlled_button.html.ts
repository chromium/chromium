// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ControlledButtonElement} from './controlled_button.js';

export function getHtml(this: ControlledButtonElement) {
  return html`<!--_html_template_start_-->
<cr-button id="button" class="${this.actionClass_}"
    ?disabled="${!this.buttonEnabled_()}">
  ${this.label}
</cr-button>

${this.hasPrefPolicyIndicator() ? html`
  <cr-policy-pref-indicator .pref="${this.pref}"
      @click="${this.onIndicatorClick_}" icon-aria-label="${this.label}">
  </cr-policy-pref-indicator>
` : ''}
<!--_html_template_end_-->`;
}
