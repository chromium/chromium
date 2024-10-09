// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {ManagedUserProfileNoticeAppElement} from './managed_user_profile_notice_app.js';

export function getHtml(this: ManagedUserProfileNoticeAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${this.useUpdatedUi_ ? html`
  <div class="main-container tangible-sync-style">
    <img class="tangible-sync-style-left-banner" alt="">
    <img class="tangible-sync-style-right-banner" alt="">
    <div id="content-container">
      ${this.showValueProposition_ ? html`
        <managed-user-profile-notice-value-prop id="value-prop"
            title="$i18n{valuePropositionTitle}"
            subtitle="$i18n{valuePropSubtitle}"
            picture-url="${this.pictureUrl_}"
            email="${this.email_}" account-name="${this.accountName_}">
        ` : ''}
        </managed-user-profile-notice-value-prop>
      ${this.showDisclosure_ ? html`
        <managed-user-profile-notice-disclosure id="disclosure"
            title="${this.title_}" subtitle="${this.subtitle_}"
            picture-url="${this.pictureUrl_}"
            ?show-enterprise-badge="${this.showEnterpriseBadge_}">
        </managed-user-profile-notice-disclosure>
      ` : ''}
      ${this.showProcessing_ ? html`
        <managed-user-profile-notice-state id="processing"
            subtitle="${this.processingSubtitle_}" icon="cr:domain">
              <div class="spinner"></div>
        </managed-user-profile-notice-state>
      ` : ''}
      ${this.showSuccess_ ? html`
        <managed-user-profile-notice-state id="success" icon="cr:domain"
            title="$i18n{successTitle}" subtitle="$i18n{successSubtitle}">
          <img class="success-icon" alt="">
        </managed-user-profile-notice-state>
      ` : ''}
      ${this.showTimeout_ ? html`
        <managed-user-profile-notice-state id="timeout" icon="cr:domain"
            title="$i18n{timeoutTitle}" subtitle="$i18n{timeoutSubtitle}">
          <img class="timeout-icon" alt="">
        </managed-user-profile-notice-state>
      ` : ''}
      ${this.showError_ ? html`
        <managed-user-profile-notice-state id="error" icon="cr:domain"
            title="$i18n{errorTitle}" subtitle="$i18n{errorSubtitle}">
          <img class="error-icon" alt="">
        </managed-user-profile-notice-state>
      ` : ''}
      ${this.showUserDataHandling_ ? html`
        <managed-user-profile-notice-data-handling id="user-data-handling"
            title="$i18n{separateBrowsingDataTitle}"
            .selectedDataHandling="${this.selectedDataHandling_}"
            @selected-data-handling-changed="${this.onDataHandlingChanged_}">
        </managed-user-profile-notice-data-handling>
      ` : ''}
    </div>
  </div>
  <div class="action-container tangible-sync-style">
    <cr-button id="proceed-button" class="action-button"
        @click="${this.onProceed_}" ?autofocus="${this.isModalDialog_}"
        ?disabled="${!this.allowProceedButton_()}"
        ?hidden="${this.showProcessing_}">
      ${this.proceedLabel_}
    </cr-button>
    <cr-button id="cancel-button" @click="${this.onCancel_}"
        ?hidden="${!this.allowCancel_()}">
      ${this.cancelLabel_}
    </cr-button>
  </div>
` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
