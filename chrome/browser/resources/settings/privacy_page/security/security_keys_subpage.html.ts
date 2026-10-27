// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SecurityKeysSubpageElement} from './security_keys_subpage.js';

export function getHtml(this: SecurityKeysSubpageElement) {
  return html`<!--_html_template_start_-->
<settings-subpage page-title="$i18n{securityKeysTitle}"
    route-path="${this.routePath}">
  <cr-link-row
      id="setPINButton"
      class="hr"
      label="$i18n{securityKeysSetPIN}"
      sub-label="$i18n{securityKeysSetPINDesc}"
      @click="${this.onSetPinClick_}"></cr-link-row>
  <cr-link-row
      id="credentialManagementButton"
      class="hr"
      label="$i18n{securityKeysCredentialManagementLabel}"
      sub-label="$i18n{securityKeysCredentialManagementDesc}"
      @click="${this.onCredentialManagementClick_}"></cr-link-row>
  ${this.enableBioEnrollment_ ? html`
    <cr-link-row
        id="bioEnrollButton"
        class="hr"
        label="$i18n{securityKeysBioEnrollmentSubpageLabel}"
        sub-label="$i18n{securityKeysBioEnrollmentSubpageDescription}"
        @click="${this.onBioEnrollClick_}">
    </cr-link-row>
  ` : ''}
  <cr-link-row
      id="resetButton"
      class="hr"
      label="$i18n{securityKeysReset}"
      sub-label="$i18n{securityKeysResetDesc}"
      @click="${this.onResetClick_}"></cr-link-row>

  ${this.showSetPINDialog_ ? html`
    <settings-security-keys-set-pin-dialog @close="${this.onSetPinDialogClose_}">
    </settings-security-keys-set-pin-dialog>
  ` : ''}

  ${this.showCredentialManagementDialog_ ? html`
    <settings-security-keys-credential-management-dialog
        @credential-management-set-pin="${this.onCredentialManagementSetPin_}"
        @close="${this.onCredentialManagementDialogClose_}">
    </settings-security-keys-credential-management-dialog>
  ` : ''}

  ${this.showResetDialog_ ? html`
    <settings-security-keys-reset-dialog @close="${this.onResetDialogClose_}">
    </settings-security-keys-reset-dialog>
  ` : ''}

  ${this.showBioEnrollDialog_ ? html`
    <settings-security-keys-bio-enroll-dialog
        @bio-enroll-set-pin="${this.onBioEnrollSetPin_}"
        @close="${this.onBioEnrollDialogClose_}">
    </settings-security-keys-bio-enroll-dialog>
  ` : ''}
</settings-subpage>
<!--_html_template_end_-->`;
}
