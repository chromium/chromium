// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsGmailOtpDisclaimerDialogElement} from './gmail_otp_disclaimer_dialog.js';

export function getHtml(this: SettingsGmailOtpDisclaimerDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" show-on-attach close-text="$i18n{close}">
  <div slot="title">$i18n{gmailOtpRequiredTitle}</div>
  <div slot="body">
    <ol>
      <li>$i18nRaw{gmailOtpRequiredStep1}</li>
      <li>$i18nRaw{gmailOtpRequiredStep2}</li>
    </ol>
  </div>
  <div slot="button-container">
    <cr-button id="confirmButton" class="action-button"
        @click="${this.onConfirmButtonClick_}">
      $i18n{gotIt}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
