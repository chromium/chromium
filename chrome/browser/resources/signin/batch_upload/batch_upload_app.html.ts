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
    <div id="title">Save data to account</div>
    <div id="subtitle">
      ${this.getDialogSubtitle_()} and other items are saved only to this device. To use them on your other devices, save them in your account.
    </div>

    <div id="account-info-row">
      <img id="account-icon" alt="Account icon"
        src="chrome://theme/IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE">
      <div id="email">elisa.g.beckett@gmail.com</div>
    </div>
  </div>

  <div id="data-sections">
    ${this.dataSections_.map((section, sectionIndex) =>
    html`
    <div class="data-section">
      <div class="data-section-header">
        <div class="data-section-title">${this.getSectionTitle_(section)}</div>
        <cr-expand-button class="expand-button" no-hover
            @click="${this.onExpandClicked_}"
            data-index="${sectionIndex}">
        </cr-expand-button>
        <div class="separator"></div>
        <cr-toggle class="toggle" checked></cr-toggle>
      </div>
      <cr-collapse class="data-items-collapse"
            data-index="${sectionIndex}"
            .opened="${this.dataSectionsExpanded_[sectionIndex]}">
        <div class="data-items-list">
          ${section.dataItems.map((item, itemIndex) =>
          html`
          <div class="data-item">
            <cr-checkbox class="item-checkbox" checked
                data-index="${itemIndex}"
                @change="${this.onCheckedChanged_}"/>
            <div class="data-item-content">
              <img class="item-icon" alt="Item icon" src="${item.iconUrl}">
              <div class="item-title">${item.title}</div>
              <div class="item-subtitle">${item.subtitle}</div>
            </div>
          </div>
          `)}
        </div>
      </cr-collapse>
    </div>
    `)}
  </div>

  <div id="action-row" class="action-container">
    <cr-button id='close-button' @click="${this.close_}">Close</cr-button>
    <cr-button id='save-button' class="action-button" @click="${this.close_}">
      Save in account
    </cr-button>
  </div>

</div>`;
  // clang-format on
}
