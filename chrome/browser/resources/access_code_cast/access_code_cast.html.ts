// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AccessCodeCastElement} from './access_code_cast.js';

export function getHtml(this: AccessCodeCastElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog">
  <div slot="title" class="title-1">$i18n{dialogTitle}</div>
  <div slot="body">
    <div id="codeInputView">
      <div class="body-1">
        $i18n{accessCodeMessage}
        <a href="$i18n{learnMoreUrl}" target="_blank">$i18n{learnMore}</a>
      </div>
      <div class="space-2"></div>
      <div class="center-content">
        <c2c-passcode-input aria-label="${this.inputLabel}"
            ?disabled="${!this.canCast}" id="codeInput" length="6"
            .value="${this.accessCode}"
            @value-changed="${this.onAccessCodeChanged}">
        </c2c-passcode-input>
      </div>
      <div class="space-1"></div>
      ${this.qrScannerEnabled ? html`
        <div class="center-content">
          <cr-button @click="${this.switchToQrInput}"
              class="center text-button">
            <cr-icon class="button-image" icon="cr:videocam"></cr-icon>
            $i18n{useCamera}
          </cr-button>
        </div>
      ` : ''}
    </div>
    <div id="qrInputView">
      <div>Camera input view</div>
    </div>
    <div id="error-message-container">
      <c2c-error-message id="errorMessage"></c2c-error-message>
    </div>
    <div class="space-1"></div>
    ${this.rememberDevices ? html`
      <div id="remembered-device-footnote">
        <cr-icon icon="cr:domain" id="remembered-device-icon"></cr-icon>
        <div id="remembered-device-content">${this.managedFootnote}</div>
      </div>
    ` : ''}
  </div>
  <div slot="button-container" id="buttons">
<if expr="not is_win">
      <cr-button @click="${this.cancelButtonPressed}">
        $i18n{cancel}
      </cr-button>
</if>
      <cr-button id="castButton" @click="${this.addSinkAndCast}"
          class="action-button" ?disabled="${this.submitDisabled}">
        $i18n{cast}
      </cr-button>
      <cr-button id="backButton" @click="${this.switchToCodeInput}"
          class="action-button">
        $i18n{back}
      </cr-button>
<if expr="is_win">
      <cr-button @click="${this.cancelButtonPressed}">
        $i118n{cancel}
      </cr-button>
</if>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
  // clang-format on
}
