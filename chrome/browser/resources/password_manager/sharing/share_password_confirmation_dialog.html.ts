// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {ConfirmationDialogStage} from './share_password_confirmation_dialog.js';
import type {SharePasswordConfirmationDialogElement} from './share_password_confirmation_dialog.js';

export function getHtml(this: SharePasswordConfirmationDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" close-text="$i18n{close}" show-on-attach no-cancel>
  <div slot="title">
    <share-password-dialog-header id="header" aria-live="polite">
      ${this.getDialogTitle_()}
    </share-password-dialog-header>
  </div>
  <div slot="body">
    <div class="animation-container" id="animation"
        ?canceled="${this.isStage_(ConfirmationDialogStage.CANCELED)}"
        ?success="${this.isStage_(ConfirmationDialogStage.SUCCESS)}">
      <img id="senderAvatar" src="${this.avatarImage}" draggable="false"
          aria-hidden="true">
      <share-password-group-avatar .members="${this.recipients}"
          id="recipientAvatar">
      </share-password-group-avatar>
      <div id="lock">
        <img src="../images/password_sharing_secure_lock.svg"
            aria-hidden="true">
      </div>
      <div id="progress">
        <img src="../images/password_sharing_progress_bar.svg"
            aria-hidden="true">
      </div>
      <site-favicon id="favicon" url="${this.iconUrl}"
          domain="${this.passwordName}" aria-hidden="true">
      </site-favicon>
    </div>
    <div id="description"
        ?hidden="${!this.isStage_(ConfirmationDialogStage.SUCCESS)}"
        .innerHTML="${this.getSuccessDescription_()}">
    </div>
  </div>
  <div slot="button-container">
    <cr-button class="cancel-button" @click="${this.onCancelClick_}" id="cancel"
        ?hidden="${!this.isStage_(ConfirmationDialogStage.LOADING)}">
      $i18n{cancel}
    </cr-button>
    <cr-button class="action-button" @click="${this.onDoneClick_}" id="done"
        ?hidden="${this.isStage_(ConfirmationDialogStage.LOADING)}">
      $i18n{done}
    </cr-button>
  </div>
  <div slot="footer" id="footerDescription" @click="${this.onFooterClick_}"
      ?hidden="${!this.isStage_(ConfirmationDialogStage.SUCCESS)}"
      .innerHTML="${this.getFooterDescription_()}">
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
