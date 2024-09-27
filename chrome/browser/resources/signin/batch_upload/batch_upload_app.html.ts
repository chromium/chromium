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
    <div id="subtitle">${this.dialogSubtitle_}</div>

    <div id="account-info-row">
      <img id="account-icon" alt="Account icon"
        src="${this.accountInfo_.dataPictureUrl}">
      <div id="email">${this.accountInfo_.email}</div>
    </div>
  </div>

  <div id="dataContainer" class="custom-scrollbar">
    <div id="dataSections">
      ${this.dataSections_.map((section, sectionIndex) =>
      html`
      <data-section .dataContainer="${section}" data-index="${sectionIndex}"
          @update-view-height="${this.updateViewHeight_}"
          @toggle-changed="${this.onSectionToggleChanged_}">
      </data-section>
      `)}
    </div>
  </div>

  <div id="action-row" class="action-container">
    <cr-button id='cancelButton' @click="${this.close_}">
      ${this.i18n('cancel')}
    </cr-button>
    <cr-button id='saveButton' class="action-button"
        ?disabled="${!this.isSaveEnabled_}"
        @click="${this.saveToAccount_}">
      ${this.i18n('saveToAccount')}
    </cr-button>
  </div>

</div>`;
  // clang-format on
}
