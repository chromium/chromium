// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsCreditCardEditDialogElement} from './credit_card_edit_dialog.js';

export function getHtml(this: SettingsCreditCardEditDialogElement) {
  return html`<!--_html_template_start_-->
    <cr-dialog id="dialog" close-text="$i18n{close}">
      <div slot="title">${this.title_}</div>
      <div slot="body">
        <cr-input id="numberInput" label="$i18n{creditCardNumber}"
            @blur="${this.onNumberInputBlur_}"
            ?invalid="${this.showErrorForCardNumber_()}"
            error-message="$i18n{creditCardNumberInvalid}"
            .value="${this.rawCardNumber_}"
            @value-changed="${this.onRawCardNumberValueChanged_}" autofocus>
        </cr-input>
        <!-- aria-hidden for creditCardExpiration label since
          creditCardExpirationMonth and creditCardExpirationYear provide
          sufficient labeling. -->
        <label id="expiration" class="cr-form-field-label" aria-hidden="true">
          $i18n{creditCardExpiration}
        </label>
        <select class="md-select" id="month" .value="${this.expirationMonth_}"
            @change="${this.onMonthChange_}"
            aria-label="$i18n{creditCardExpirationMonth}"
            aria-invalid="${this.getExpirationAriaInvalid_()}">
          ${this.monthList_.map(item => html`
            <option>${item}</option>
          `)}
        </select>
        <select class="md-select" id="year" .value="${this.expirationYear_}"
            @change="${this.onYearChange_}"
            aria-label="$i18n{creditCardExpirationYear}"
            aria-invalid="${this.getExpirationAriaInvalid_()}">
          ${this.yearList_.map(item => html`
            <option>${item}</option>
          `)}
        </select>
        <div id="expiredError">$i18n{creditCardExpired}</div>
        ${this.checkIfCvcStorageIsAvailable_() ? html`
          <cr-input id="cvcInput" label="$i18n{creditCardCvcInputTitle}"
              placeholder="$i18n{creditCardCvcInputPlaceholder}"
              .value="${this.cvc_}" @value-changed="${this.onCvcValueChanged_}">
            <img slot="suffix" id="cvcImage"
                src="${this.getCvcImageSource_()}"
                title="${this.getCvcImageTooltip_()}">
          </cr-input>
        ` : ''}
        <!-- Place cardholder name field and nickname field after CVC input.-->
        <cr-input id="nameInput" label="$i18n{creditCardName}"
            .value="${this.name_}" @value-changed="${this.onNameValueChanged_}"
            spellcheck="false">
        </cr-input>
        <cr-input id="nicknameInput" label="$i18n{creditCardNickname}"
            .value="${this.nickname_}"
            @value-changed="${this.onNicknameValueChanged_}"
            spellcheck="false" maxlength="25"
            ?invalid="${this.nicknameInvalid_}"
            error-message="$i18n{creditCardNicknameInvalid}"
            aria-description="${this.i18n('inputMaxLengthDescription', 25)}">
          <div id="charCount" slot="suffix" ?hidden="${!this.nickname_}">
            ${this.nickname_.length}/25
          </div>
        </cr-input>
        <div id="saved-to-this-device-only-label">
          $i18n{savedToThisDeviceOnly}
        </div>
      </div>
      <div slot="button-container">
        <cr-button id="cancelButton" class="cancel-button"
            @click="${this.onCancelButtonClick_}">$i18n{cancel}</cr-button>
        <cr-button id="saveButton" class="action-button"
            @click="${this.onSaveButtonClick_}"
            ?disabled="${!this.saveEnabled_()}">
          $i18n{save}
        </cr-button>
      </div>
    </cr-dialog>
<!--_html_template_end_-->`;
}
