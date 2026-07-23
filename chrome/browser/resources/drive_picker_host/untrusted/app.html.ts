// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {DrivePickerHostUntrustedAppElement} from './app.js';

export function getHtml(this: DrivePickerHostUntrustedAppElement) {
  return html`<!--_html_template_start_-->
<div id="picker-container" ?hidden="${this.showErrorScreen_}"></div>
${this.showErrorScreen_ ? html`
  <div id="error-screen">
    <div class="error-title">
      ${this.i18n('driveDisclaimerError')}
    </div>
    <div class="error-buttons">
      <cr-button class="cancel-button" @click="${this.onCancelClick_}">
        ${this.i18n('cancel')}
      </cr-button>
      <cr-button class="action-button" @click="${this.onTryAgainClick_}">
        ${this.i18n('driveDisclaimerTryAgain')}
      </cr-button>
    </div>
  </div>
` : ''}
<!--_html_template_end_-->`;
}
