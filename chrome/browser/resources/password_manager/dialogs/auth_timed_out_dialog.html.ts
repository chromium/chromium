// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {AuthTimedOutDialogElement} from './auth_timed_out_dialog.js';

export function getHtml(this: AuthTimedOutDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog">
  <div slot="title" class="dialog-title">${this.getTitle_()}</div>
  <div slot="body">
    <div>$i18n{authTimedOutDescription}</div>
  </div>
  <div slot="button-container">
    <cr-button class="action-button" autofocus
        @click="${this.onCloseButtonClick_}">
      $i18n{gotIt}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
