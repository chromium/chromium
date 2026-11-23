// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsCreditCardEditDialogV2Element} from './credit_card_edit_dialog_v2.js';

export function getHtml(this: SettingsCreditCardEditDialogV2Element) {
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" close-text="$i18n{close}">
  <div id="title" slot="title">
    <span>${this.title_}</span>
<if expr="_google_chrome">
    <picture id="paymentsIcon">
      <source srcset="${this.getGooglePayDarkModeLogoSrcSet_()}"
          media="(prefers-color-scheme: dark)">
      <img alt="" srcset="${this.getGooglePayLightModeLogoSrcSet_()}">
    </picture>
</if>
  </div>
  <div slot="body">
    <cr-input id="numberInput" class="stroked" label="$i18n{creditCardNumber}"
        @blur="${this.onNumberInputBlur_}"
        ?invalid="${this.showErrorForCardNumber_()}"
        error-message="$i18n{creditCardNumberInvalid}"
        .value="${this.rawCardNumber_}"
        @value-changed="${this.onRawCardNumberValueChanged_}" autofocus>
    </cr-input>
    <cr-input id="expirationDateInput" class="stroked"
        label="$i18n{creditCardExpiration}" placeholder="MM/YY"
        inputmode="numeric" maxlength="5"
        @blur="${this.onExpirationDateInputBlur_}"
        ?invalid="${this.showErrorForExpirationDate_()}"
        error-message="$i18n{creditCardExpired}"
        .value="${this.expirationDate_}"
        @value-changed="${this.onExpirationDateValueChanged_}">
    </cr-input>
    ${this.checkIfCvcStorageIsAvailable_() ? html`
      <cr-input id="cvcInput" class="stroked"
          label="$i18n{creditCardCvcInputTitle}"
          placeholder="$i18n{creditCardCvcInputPlaceholder}"
          .value="${this.cvc_}" @value-changed="${this.onCvcValueChanged_}">
        <img slot="suffix" id="cvcImage" src="${this.getCvcImageSource_()}"
            title="${this.getCvcImageTooltip_()}">
      </cr-input>
    ` : ''}
    <!-- Place cardholder name field and nickname field after CVC input.-->
    <cr-input id="nicknameInput" class="stroked"
        label="$i18n{creditCardNickname}" .value="${this.nickname_}"
        @value-changed="${this.onNicknameValueChanged_}" spellcheck="false"
        maxlength="25" ?invalid="${this.nicknameInvalid_}"
        error-message="$i18n{creditCardNicknameInvalid}"
        aria-description="${this.i18n('inputMaxLengthDescription', 25)}">
      <div id="charCount" slot="suffix" ?hidden="${!this.nickname_}">
        ${this.nickname_.length}/25
      </div>
    </cr-input>
    <cr-input id="nameInput" class="stroked" label="$i18n{creditCardName}"
        .value="${this.name_}" @value-changed="${this.onNameValueChanged_}"
        spellcheck="false">
    </cr-input>
    ${this.primaryAccountEmail_ ? html`
      <div id="saved-to-google-account-label">
        ${this.i18n('paymentsSavedToGoogleAccount', this.primaryAccountEmail_)}
      </div>
    ` : html`
      <div id="saved-to-this-device-only-label">
        $i18n{savedToThisDeviceOnly}
      </div>
    `}
  </div>
  <div id="footer" slot="button-container">
    <div id="profile-info">
      <div id="profile-icon"
          style="background-image: ${
              this.getIconImageSet_(this.primaryAccountIconUrl_)}">
      </div>
      <div id="profile-text" class="cr-row-gap">
        <div id="profile-name" class="text-elide">
          ${this.primaryAccountName_}
        </div>
        <div id="profile-email" class="text-elide secondary">
          ${this.primaryAccountEmail_}
        </div>
      </div>
    </div>
    <div id="button-container">
      <cr-button id="cancelButton" class="cancel-button"
          @click="${this.onCancelButtonClick_}">
        $i18n{cancel}
      </cr-button>
      <cr-button id="saveButton" class="action-button"
          @click="${this.onSaveButtonClick_}"
          ?disabled="${!this.saveEnabled_()}">
        $i18n{save}
      </cr-button>
    </div>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
