// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {BatchUploadAppElement} from './batch_upload_app.js';

export function getHtml(this: BatchUploadAppElement) {
  // clang-format off
  return html`
<div id="batchUploadDialog">
  <div id="header">
    <div id="title">${this.i18n('batchUploadTitle')}</div>
    <div id="subtitle">
      ${this.getDialogSubtitle_()}
    </div>

    <div id="account-info-row">
      <img id="account-icon" alt="Account icon"
        src="chrome://theme/IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE">
      <div id="email">elisa.g.beckett@gmail.com</div>
    </div>
  </div>

  <div id="data-sections">
    ${this.dataSections_.map((section) =>
    html`
    <data-section .dataContainer="${section}">
    </data-section>
    `)}
  </div>

  <div id="action-row" class="action-container">
    <cr-button id='close-button' @click="${this.close_}">
      ${this.i18n('cancel')}
    </cr-button>
    <cr-button id='save-button' class="action-button"
        @click="${this.saveToAccount_}">
      ${this.i18n('saveToAccount')}
    </cr-button>
  </div>

</div>`;
  // clang-format on
}
