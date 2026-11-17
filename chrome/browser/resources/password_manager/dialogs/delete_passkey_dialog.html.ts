// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {DeletePasskeyDialogElement} from './delete_passkey_dialog.js';

export function getHtml(this: DeletePasskeyDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" close-text="$i18n{close}" ignore-enter-key
    show-on-attach>
  <div slot="title" class="dialog-title">
    $i18n{deletePasskeyConfirmationTitle}
  </div>
  <div slot="body">
    <span id="link" .innerHTML="${this.getDescriptionHtml_()}"></span>
  </div>
  <div slot="button-container">
    <cr-button class="cancel-button" @click="${this.onCancelClick_}"
        id="cancelButton" autofocus>
      $i18n{cancel}
    </cr-button>
    <cr-button class="action-button" @click="${this.onDeleteClick_}"
        id="deleteButton">
      $i18n{delete}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
