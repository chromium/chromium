// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SignatureSelectorElement} from './signature_selector.js';

export function getHtml(this: SignatureSelectorElement) {
  // TODO(crbug.com/573256245): Add signature dropdown menu and deletion, update
  // the icon, and replace placeholder string.
  return html`<!--_html_template_start_-->
<cr-button id="new-signature" @click="${this.onNewSignatureClick_}">
  <cr-icon icon="pdf:add" slot="prefix-icon"></cr-icon>
  New signature
</cr-button>
<!--_html_template_end_-->`;
}
