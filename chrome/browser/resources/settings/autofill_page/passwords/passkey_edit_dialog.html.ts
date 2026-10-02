// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {PasskeyEditDialogElement} from './passkey_edit_dialog.js';

export function getHtml(this: PasskeyEditDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" close-text="$i18n{close}" show-on-attach>
  <div slot="title" id="title">$i18n{editPasskeyDialogTitle}</div>
  <div slot="body">
    <cr-input id="websiteInput" label="$i18n{editPasskeySiteLabel}"
        placeholder="${this.relyingPartyId}" readonly="true">
    </cr-input>
    <cr-input id="usernameInput" label="$i18n{editPasskeyUsernameLabel}"
        .value="${this.username}"
        @value-changed="${this.onUsernameValueChanged_}"
        ?invalid="${this.usernameInputInvalid_}"
        error-message="${this.usernameInputErrorMessage_}">
    </cr-input>
    <div id="footnote">${this.dialogFootnote_}</div>
  </div>
  <div slot="button-container">
    <cr-button id="cancel" class="cancel-button"
        @click="${this.onCancelClick_}">
      $i18n{cancel}
    </cr-button>
    <cr-button id="actionButton" class="action-button"
        @click="${this.onSaveButtonClick_}"
        ?disabled="${this.usernameInputInvalid_}">
      $i18n{save}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
