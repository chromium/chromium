// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {AddPasswordDialogElement} from './add_password_dialog.js';

export function getHtml(this: AddPasswordDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" show-on-attach>
  <div slot="title" id="title" class="dialog-title">
    $i18n{addPasswordTitle}
  </div>
  <div slot="body">
    <select class="md-select" id="storePicker"
        aria-description="$i18n{addPasswordStorePickerA11yDescription}"
        ?hidden="${!this.isAccountStoreUser}">
      <option value="${this.storeOptionAccountValue_}">
        $i18n{addPasswordStoreOptionAccount}
      </option>
      <option value="${this.storeOptionDeviceValue_}">
        $i18n{addPasswordStoreOptionDevice}
      </option>
    </select>
    <cr-input id="websiteInput" label="$i18n{websiteLabel}" autofocus required
        placeholder="example.com" .value="${this.website_}"
        ?invalid="${this.isWebsiteInputInvalid_()}"
        error-message="${this.getWebsiteErrorMessage_()}" spellcheck="false"
        @input="${this.onWebsiteInput_}" @blur="${this.onWebsiteInputBlur_}">
    </cr-input>
    <cr-input id="usernameInput" label="$i18n{usernameLabel}"
        .value="${this.username_}" @input="${this.onUsernameInput_}"
        spellcheck="false" ?invalid="${this.doesUsernameExistAlready_()}"
        error-message="${this.usernameErrorMessage_ || ''}">
    </cr-input>
    <a id="viewExistingPasswordLink" is="action-link" href="/"
        @click="${this.onViewExistingPasswordLinkClick_}"
        aria-description="${this.getViewExistingPasswordAriaDescription_()}"
        ?hidden="${!this.doesUsernameExistAlready_()}">
      $i18n{viewExistingPassword}
    </a>
    <cr-input id="passwordInput" label="$i18n{passwordLabel}"
        type="${this.getPasswordInputType()}" .value="${this.password_}"
        ?invalid="${this.isPasswordInvalid_}"
        @blur="${this.onPasswordInputBlur_}" @input="${this.onPasswordInput_}"
        required class="password-input" spellcheck="false">
      <cr-icon-button id="showPasswordButton" slot="inline-suffix"
          class="${this.getShowHideButtonIconClass()}"
          title="${this.getShowHideButtonLabel()}"
          @click="${this.onShowHidePasswordButtonClick}">
      </cr-icon-button>
    </cr-input>
    <div id="footnote">
      $i18n{addPasswordFooter}
    </div>
    <div class="divider"></div>
    <cr-textarea label="$i18n{noteLabel}" id="noteInput" .value="${this.note_}"
        @input="${this.onNoteInput_}" ?invalid="${this.isNoteInputInvalid_()}"
        autogrow first-footer="${this.getFirstNoteFooter_()}"
        second-footer="${this.getSecondNoteFooter_()}">
    </cr-textarea>
  </div>
  <div slot="button-container">
    <cr-button id="cancel" class="cancel-button"
        @click="${this.onCancelClick_}">
      $i18n{cancel}
    </cr-button>
    <cr-button id="addButton" class="action-button"
        ?disabled="${this.isAddButtonDisabled_()}"
        @click="${this.onAddButtonClick_}">
      $i18n{save}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
