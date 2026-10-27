// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SecureDnsInputElement} from './secure_dns_input.js';

export function getHtml(this: SecureDnsInputElement) {
  return html`<!--_html_template_start_-->

    <!-- Max length of 100 KB to prevent browser from freezing. -->
    <cr-textarea id="input" .value="${this.value}"
        @value-changed="${this.onValueChanged_}" rows="1" autogrow
        placeholder="$i18n{secureDnsCustomPlaceholder}" ?invalid="${this.isInvalid_()}"
        first-footer="${this.errorText_}" maxlength="102400" spellcheck="false"
        @keypress="${this.onKeypress_}" @input="${this.onInput_}"
        @blur="${this.onInputBlur_}" @change="${this.onInputChange_}">
    </cr-textarea>
<!--_html_template_end_-->`;
}
