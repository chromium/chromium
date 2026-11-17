// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {DisconnectCloudAuthenticatorDialogElement} from './disconnect_cloud_authenticator_dialog.js';

export function getHtml(this: DisconnectCloudAuthenticatorDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" show-on-attach>
  <div slot="title" class="dialog-title">
    $i18n{disconnectCloudAuthenticatorConfirmationDialogTitle}
  </div>
  <div slot="body">
    $i18n{disconnectCloudAuthenticatorConfirmationDialogDescription}
  </div>
  <div slot="button-container">
    <cr-button class="cancel-button" @click="${this.onCancelClick_}"
        id="cancelButton" autofocus>
      $i18n{cancel}
    </cr-button>
    <cr-button class="action-button" @click="${this.onConfirmClick_}"
        id="confirmButton">
      $i18n{confirm}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
