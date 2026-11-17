// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SharePasswordDialogHeaderElement} from './share_password_dialog_header.js';

export function getHtml(this: SharePasswordDialogHeaderElement) {
  return html`<!--_html_template_start_-->
<span class="elide-text-after-three-lines">
  <slot></slot>
</span>
<cr-icon-button iron-icon="cr:help" id="helpButton" title="$i18n{help}"
    @click="${this.onHelpClick_}" suppress-rtl-flip>
</cr-icon-button>
<!--_html_template_end_-->`;
}
