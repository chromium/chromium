// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsIbanEditDialogElement} from './iban_edit_dialog.js';

export function getHtml(this: SettingsIbanEditDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" close-text="$i18n{close}">
  <div slot="title">${this.title_}</div>
  <div slot="body">
    <cr-input id="valueInput" label="$i18n{addPaymentMethodIban}"
        @blur="${this.onIbanInputBlur_}"
        ?invalid="${this.showErrorForIban_()}"
        error-message="$i18n{ibanInvalid}"
        .value="${this.value_}" @value-changed="${this.onValueChanged_}"
        autofocus>
    </cr-input>
    <cr-input id="nicknameInput" label="$i18n{ibanNickname}"
        .value="${this.nickname_}"
        @value-changed="${this.onNicknameValueChanged_}"
        spellcheck="false" maxlength="25"
        aria-description="${this.i18n('inputMaxLengthDescription', 25)}">
      <div id="charCount" slot="suffix" ?hidden="${!this.nickname_}">
        ${this.computeNicknameCharCount_()}/25
      </div>
    </cr-input>
    <div id="saved-to-this-device-only-label">
      $i18n{ibanSavedToThisDeviceOnly}
    </div>
  </div>
  <div slot="button-container">
    <cr-button id="cancelButton" class="cancel-button"
        @click="${this.onCancelButtonClick_}">$i18n{cancel}</cr-button>
    <cr-button id="saveButton" class="action-button"
        ?disabled="${!this.isIbanValid_()}"
        @click="${this.onIbanSaveButtonClick_}">
      $i18n{save}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
