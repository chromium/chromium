// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {EditPasswordDisclaimerDialogElement} from './edit_password_disclaimer_dialog.js';

export function getHtml(this: EditPasswordDisclaimerDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" close-text="$i18n{close}">
  <div slot="title">${this.getDisclaimerTitle_()}</div>
  <div slot="body">${this.getDisclaimerDescription_()}</div>
  <div slot="button-container">
    <cr-button id="cancel" class="cancel-button" @click="${this.onCancelClick_}"
        autofocus>
      $i18n{cancel}
    </cr-button>
    <cr-button id="edit" class="action-button" @click="${this.onEditClick_}">
      $i18n{editPassword}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
