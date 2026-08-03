// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ExceptionAddInputElement} from './exception_add_input.js';

export function getHtml(this: ExceptionAddInputElement) {
  return html`<!--_html_template_start_-->
<cr-input id="input" label="$i18n{addSite}" aria-label="$i18n{addSiteTitle}"
    placeholder="example.com" .value="${this.rule}"
    @value-changed="${this.onRuleValueChanged_}"
    error-message="${this.errorMessage}" ?invalid="${this.inputInvalid}"
    spellcheck="false" autofocus>
</cr-input>
<!--_html_template_end_-->`;
}
