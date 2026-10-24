// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ViewerSignatureDialogElement} from './viewer_signature_dialog.js';

export function getHtml(this: ViewerSignatureDialogElement) {
  // TODO(crbug.com/568576345): Add translated strings.
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" show-on-attach>
  <div slot="title">New signature or initials</div>
  <div slot="header">
    <cr-icon-button id="close" iron-icon="cr:close"
        aria-label="Close signature inkpad" title="Close signature inkpad"
        @click="${this.onCloseClick_}">
    </cr-icon-button>
  </div>
  <div slot="button-container">
    <cr-button id="clear" class="tonal-button" aria-label="Clear signature">
      Clear
    </cr-button>
    <cr-button id="create" class="action-button" aria-label="Create signature">
      Create
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
