// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsSecurityKeysResetDialogElement} from './security_keys_reset_dialog.js';

export function getHtml(this: SettingsSecurityKeysResetDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" close-text="$i18n{close}" ignore-popstate
    @close="${this.onDialogClose_}">
  <div slot="title">${this.title_}</div>
  <div slot="body">
    <cr-page-selector attr-for-selected="id" .selected="${this.shown_}"
        @iron-select="${this.onIronSelect_}">
      <div id="initial">
        <p>$i18n{securityKeysResetStep1}</p>
        <div class="spinner"></div>
      </div>

      <div id="noReset">
        <p>$i18n{securityKeysNoReset}</p>
      </div>

      <div id="resetFailed">
        <p>${this.resetFailed_()}</p>
      </div>

      <div id="resetConfirm">
        <p>$i18n{securityKeysResetStep2}</p>
      </div>

      <div id="resetSuccess">
        <p>$i18n{securityKeysResetSuccess}</p>
      </div>

      <div id="resetNotAllowed">
        <p>$i18n{securityKeysResetNotAllowed}</p>
      </div>
    </cr-page-selector>
  </div>
  <div slot="button-container">
    <cr-button id="button" class="${this.maybeActionButton_()}"
        @click="${this.onButtonClick_}">
      ${this.closeText_()}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
