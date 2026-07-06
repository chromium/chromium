// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {HomeUrlInputElement} from './home_url_input.js';

export function getHtml(this: HomeUrlInputElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<!-- Max length of 100 KB to prevent browser from freezing. -->
<cr-input id="input"
    .value="${this.value}"
    @value-changed="${this.onValueChanged_}"
    ?invalid="${this.invalid}"
    @invalid-changed="${this.onInvalidChanged_}"
    ?disabled="${this.isDisabled_()}"
    input-tabindex="${this.getTabindex_()}"
    error-message="$i18n{notValid}"
    placeholder="$i18n{enterCustomWebAddress}"
    maxlength="102400"
    spellcheck="false"
    @change="${this.onChange_}"
    @input="${this.onInput_}"
    @keydown="${this.onKeydown_}"
    @keypress="${this.onKeypress_}"
    @keyup="${this.onKeyup_}">
  ${this.hasPrefPolicyIndicator() ? html`
    <cr-policy-pref-indicator slot="suffix"
        .pref="${this.pref}" icon-aria-label="${this.label}">
    </cr-policy-pref-indicator>
  ` : ''}
</cr-input>
<!--_html_template_end_-->`;
  // clang-format on
}
