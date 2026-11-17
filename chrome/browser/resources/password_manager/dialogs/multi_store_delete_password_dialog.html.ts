// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {MultiStoreDeletePasswordDialogElement} from './multi_store_delete_password_dialog.js';

export function getHtml(this: MultiStoreDeletePasswordDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" close-text="$i18n{close}">
  <div slot="title" class="dialog-title">
    $i18n{deletePasswordDialogAccount}
  </div>
  <div slot="body">
    <div .innerHTML="${this.getDialogBodyMessage_()}"></div>
    <cr-checkbox ?checked="${this.removeFromAccountChecked_}"
        @checked-changed="${this.onRemoveFromAccountCheckedChanged_}"
        id="removeFromAccountCheckbox">
      <div class="cr-row first">
        <img id="avatar" src="${this.avatarImage}">
        <div>
          $i18n{deletePasswordDialogAccount}
        </div>
        <div class="cr-secondary-text">
          &nbsp;(${this.accountEmail})
        </div>
      </div>
    </cr-checkbox>
    <cr-checkbox ?checked="${this.removeFromDeviceChecked_}"
        @checked-changed="${this.onRemoveFromDeviceCheckedChanged_}"
        id="removeFromDeviceCheckbox">
      <div>
        $i18n{deletePasswordDialogDevice}
      </div>
    </cr-checkbox>
  </div>
  <div slot="button-container">
    <cr-button class="cancel-button" id="cancelButton"
        @click="${this.onCancelButtonClick_}" autofocus>
      $i18n{cancel}
    </cr-button>
    <cr-button class="action-button" id="removeButton"
        ?disabled="${this.shouldDisableRemoveButton_()}"
        @click="${this.onRemoveButtonClick_}">
      $i18n{deletePassword}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
