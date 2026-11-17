// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {DeletePasswordDisclaimerDialogElement} from './delete_password_disclaimer_dialog.js';

export function getHtml(this: DeletePasswordDisclaimerDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" close-text="$i18n{close}" ignore-popstate
    ignore-enter-key>
  <div slot="title" class="dialog-title">
    $i18n{deletePasswordConfirmationTitle}
  </div>
  <div slot="body">
    <span id="link" ?hidden="${!this.hasSecureChangePasswordUrl_()}"
        .innerHTML="${this.getDescriptionHtml_()}">
    </span>
    <span id="text" ?hidden="${this.hasSecureChangePasswordUrl_()}">
      ${this.getDescriptionText_()}
    </span>
  </div>
  <div slot="button-container">
    <cr-button class="cancel-button" @click="${this.onCancelClick_}" id="cancel"
        autofocus>
      $i18n{cancel}
    </cr-button>
    <cr-button class="action-button" @click="${this.onDeleteClick_}"
        id="delete">
      $i18n{deletePassword}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
