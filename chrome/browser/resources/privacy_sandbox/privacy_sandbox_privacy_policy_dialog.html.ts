// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PrivacySandboxPrivacyPolicyDialogElement} from './privacy_sandbox_privacy_policy_dialog.js';

export function getHtml(this: PrivacySandboxPrivacyPolicyDialogElement) {
  return html`
    <div class="button-container">
      <cr-icon-button id="backButton"
          aria-description="$i18n{privacyPolicyBackButtonAria}"
          iron-icon="cr:arrow-back"
          @click="${this.onBackToConsentNotice_}">
      </cr-icon-button>
    </div>
    <iframe id="privacyPolicy"
        tabindex="${this.shouldShow ? 0 : -1}"
        src='chrome-untrusted://privacy-sandbox-dialog/privacy-policy'
        frameBorder="0">
    </iframe>
  `;
}
