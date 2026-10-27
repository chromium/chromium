// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsSecurityKeysPinFieldElement} from './security_keys_pin_field.js';

export function getHtml(this: SettingsSecurityKeysPinFieldElement) {
  return html`<!--_html_template_start_-->
<p>$i18n{securityKeysPINPrompt}</p>
<cr-input id="pin" .value="${this.value_}"
    @value-changed="${this.onPinValueChanged_}"
    min-length="${this.minPinLength}" max-length="255" spellcheck="false"
    @input="${this.onPinInput_}"
    ?invalid="${this.isNonEmpty_()}"
    label="$i18n{securityKeysPIN}"
    type="${this.inputType_()}"
    error-message="${this.error_}">
  <cr-icon-button slot="suffix" id="showButton"
      class="${this.showButtonClass_()}"
      title="${this.showButtonTitle_()}"
      focus-row-control focus-type="showPassword"
      @click="${this.onShowButtonClick_}"></cr-icon-button>
</cr-input>
<!--_html_template_end_-->`;
}
