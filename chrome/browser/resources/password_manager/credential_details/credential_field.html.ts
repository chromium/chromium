// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {CredentialFieldElement} from './credential_field.js';

export function getHtml(this: CredentialFieldElement) {
  return html`<!--_html_template_start_-->
<cr-input .value="${this.value}" id="inputValue" readonly class="input-field"
    label="${this.label}" placeholder="${this.placeholder}" role="textbox"
    aria-disabled="true">
  <cr-icon-button id="copyButton" class="icon-copy-content" slot="inline-suffix"
      title="${this.copyButtonLabel}" @click="${this.onCopyValueClick_}"
      ?hidden="${!this.value}">
  </cr-icon-button>
</cr-input>
<!--_html_template_end_-->`;
}
