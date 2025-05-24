// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SpHeadingElement} from './sp_heading.js';

export function getHtml(this: SpHeadingElement) {
  return html`<!--_html_template_start_-->
<cr-icon-button id="backButton" iron-icon="cr:arrow-back"
    ?hidden="${this.hideBackButton}" ?disabled="${this.disableBackButton}"
    @click="${this.onBackButtonClick_}" aria-label="${this.backButtonAriaLabel}"
    title="${this.backButtonTitle}">
</cr-icon-button>

<slot name="heading"></slot>
<slot name="metadata"></slot>
<div class="sp-icon-buttons-row"><slot name="buttons"></slot></div>
<!--_html_template_end_-->`;
}
