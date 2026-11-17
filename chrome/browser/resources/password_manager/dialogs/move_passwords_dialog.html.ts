// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {MovePasswordsDialogElement} from './move_passwords_dialog.js';

export function getHtml(this: MovePasswordsDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog">
  <h1 slot="title" class="dialog-title">$i18n{movePasswordsDialogTitle}</h1>
  <div slot="body">
    <div id="subtitle">${this.descriptionString}</div>
    <div id="accountInfo" class="flex-centered">
      <img id="avatar" src="${this.avatarImage}">
      <div id="accountEmail" class="label">${this.accountEmail}</div>
    </div>
    ${!this.hasOnlyOneDeviceCredential ? html`
      <div id="passwordsSection">
        <h2 id="passwordsTitle" class="text-elide">${this.passwordsTitle_}</h2>
        ${this.passwords.map(item => html`
          <password-preview-item .passwordId="${item.id}"
              .username="${item.username}" .password="${item.password || ''}"
              @change="${this.onPasswordPreviewItemChange_}" checked>
          </password-preview-item>
        `)}
      </div>
    ` : ''}
  </div>
  <div id="buttonRow" slot="button-container">
    <cr-button class="tonal-button" @click="${this.onCancelClick_}">
      $i18n{cancel}
    </cr-button>
    <cr-button id="acceptButton" class="action-button"
        @click="${this.onAcceptButtonClick_}"
        ?disabled="${!this.selectedPasswordIds_.length}">
      $i18n{movePasswordsDialogSaveButton}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
