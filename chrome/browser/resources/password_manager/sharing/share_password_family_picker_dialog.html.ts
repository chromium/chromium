// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SharePasswordFamilyPickerDialogElement} from './share_password_family_picker_dialog.js';

export function getHtml(this: SharePasswordFamilyPickerDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" close-text="$i18n{close}" show-on-attach>
  <div slot="title">
    <share-password-dialog-header id="header">
      ${this.dialogTitle}
    </share-password-dialog-header>
  </div>
  <div slot="body">
    <div id="description"
        .innerHTML="${
            this.i18nAdvanced('sharePasswordFamilyPickerDescription')}">
    </div>
    ${this.eligibleRecipients_.map((item, index) => html`
      <share-password-recipient .recipient="${item}"
          ?selected="${this.shouldPreselectFirstEntry_(index)}"
          @change="${this.onRecipientChange_}">
      </share-password-recipient>
    `)}
    ${this.ineligibleRecipients_.map(item => html`
      <share-password-recipient .recipient="${item}" disabled>
      </share-password-recipient>
    `)}
  </div>
  <div slot="button-container">
    <cr-button class="cancel-button" @click="${this.onCancelClick_}"
        id="cancel">
      $i18n{cancel}
    </cr-button>
    <cr-button class="action-button" id="action" @click="${this.onShareClick_}"
        ?disabled="${!this.selectedRecipients.length}">
      $i18n{share}
    </cr-button>
  </div>
  <div slot="footer" class="flex-centered">
    <img id="avatar" src="${this.avatarImage}">
    <div id="footerDescription">
      <!-- lit-template-format-off -->
      <a href="$i18n{familyGroupViewURL}" target="_blank" id="viewFamily"
          @click="${this.onViewFamilyClick_}">
        $i18n{sharePasswordViewFamily}</a><span> • </span><span>${this.accountEmail}</span>
      <!-- lit-template-format-on -->
    </div>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
