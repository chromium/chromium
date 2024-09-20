// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {LegacyManagedUserProfileNoticeAppElement} from './legacy_managed_user_profile_notice_app.js';

export function getHtml(this: LegacyManagedUserProfileNoticeAppElement) {
  return html`<!--_html_template_start_-->
${this.useLegacyUi_ ? html`
  <div class="main-container tangible-sync-style
      ${this.getMaybeDialogClass_()}">
    <img class="tangible-sync-style-left-banner" alt="">
    <img class="tangible-sync-style-right-banner" alt="">
    <img class="tangible-sync-style-dialog-illustration" alt="">
    <div id="content-container">
      <div id="header-container">
        <div id="avatar-container">
          <img id="avatar" alt="" src="${this.pictureUrl_}">
          <div class="work-badge" ?hidden="${!this.showEnterpriseBadge_}">
            <cr-icon class="icon" icon="cr:domain"></cr-icon>
          </div>
        </div>
      </div>
      <div id="text-container">
        ${this.title_ ? html`
          <h2 class="title">${this.title_}</h2>
        ` : ''}
        ${this.subtitle_ ? html`
          <p class="subtitle">${this.subtitle_}</p>
        ` : ''}
        ${this.enterpriseInfo_ ? html `
          <div class="info-box">
            <div class="icon-container">
              <cr-icon icon="cr:domain"></cr-icon>
            </div>
            <p id="enterpriseInfo">${this.enterpriseInfo_}</p>
          </div>
        ` : ''}
      </div>
      ${this.showLinkDataCheckbox_ ? html`
        <cr-checkbox id="linkData" ?checked="${this.linkData_}"
            @checked-changed="${this.onLinkDataChanged_}">
          <div>$i18n{linkDataText}</div>
        </cr-checkbox>
      ` : ''}
    </div>
  </div>
  <div class="action-container tangible-sync-style
      ${this.getMaybeDialogClass_()}">
    <cr-button id="proceed-button" class="action-button"
        @click="${this.onProceed_}" ?autofocus="${this.isModalDialog_}"
        ?disabled="${this.disableProceedButton_}">
      ${this.proceedLabel_}
    </cr-button>
    <cr-button id="cancel-button" @click="${this.onCancel_}">
      $i18n{cancelLabel}
    </cr-button>
  </div>
` : ''}
<!--_html_template_end_-->`;
}
