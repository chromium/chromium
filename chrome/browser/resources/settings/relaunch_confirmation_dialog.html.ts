// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {RelaunchConfirmationDialogElement} from './relaunch_confirmation_dialog.js';

export function getHtml(this: RelaunchConfirmationDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" close-text="$i18n{close}" show-on-attach>
  <div slot="title">$i18n{relaunchConfirmationDialogTitle}</div>
  <div slot="body">${this.relaunchConfirmationDialogDesc}</div>
  <div slot="button-container">
    <cr-button id="cancel" class="cancel-button"
        @click="${this.onCancelClick_}">
      $i18n{cancel}
    </cr-button>
    <cr-button id="confirm" class="action-button"
        @click="${this.onConfirmClick_}">
      $i18n{restart}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
