// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import {State} from './managed_user_profile_notice_browser_proxy.js';
import type {ManagedUserProfileNoticeAppRefreshElement} from './managed_user_profile_notice_app_refresh.js';

export function getHtml(this: ManagedUserProfileNoticeAppRefreshElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="main-container">
  ${this.isState_(State.VALUE_PROPOSITION) ? html`
    <managed-user-profile-notice-value-prop id="value-prop"
        title="${this.i18n('valuePropTitle')}"
        subtitle="${this.i18n('valuePropSubtitle')}"
        picture-url="${this.profileInfo_.pictureUrl}"
        email="${this.profileInfo_.email}"
        account-name="${this.profileInfo_.accountName}"
        ?show-enterprise-badge="${this.profileInfo_.showEnterpriseBadge}">
    </managed-user-profile-notice-value-prop>
  ` : ''}
  ${this.isState_(State.DISCLOSURE) ? html`
    <managed-user-profile-notice-disclosure-refresh id="disclosure"
        title="${this.i18n('profileDisclosureTitle')}"
        subtitle="${this.i18n('profileDisclosureSubtitle')}"
        picture-url="${this.profileInfo_.pictureUrl}"
        ?show-enterprise-badge="${this.profileInfo_.showEnterpriseBadge}">
    </managed-user-profile-notice-disclosure-refresh>
  ` : ''}
  ${this.isState_(State.PROCESSING) ? html`
    <managed-user-profile-notice-state id="processing"
        subtitle="${this.processingSubtitle_}" icon="cr:domain">
      <div class="spinner"></div>
    </managed-user-profile-notice-state>
  ` : ''}
  ${this.isState_(State.SUCCESS) ? html`
    <managed-user-profile-notice-state id="success" icon="cr:domain"
        title="$i18n{successTitle}" subtitle="$i18n{successSubtitle}">
      <img class="success-icon" alt="">
    </managed-user-profile-notice-state>
  ` : ''}
  ${this.isState_(State.TIMEOUT) ? html`
    <managed-user-profile-notice-state id="timeout" icon="cr:domain"
        title="$i18n{timeoutTitle}" subtitle="$i18n{timeoutSubtitle}">
      <img class="timeout-icon" alt="">
    </managed-user-profile-notice-state>
  ` : ''}
  ${this.isState_(State.ERROR) ? html`
    <managed-user-profile-notice-state id="error" icon="cr:domain"
        title="${this.errorTitle_}" subtitle="${this.errorSubtitle_}">
      <img class="error-icon" alt="">
    </managed-user-profile-notice-state>
  ` : ''}
  ${this.isState_(State.USER_DATA_HANDLING) ? html`
    <managed-user-profile-notice-data-handling id="user-data-handling"
        title="${this.i18n('separateBrowsingDataTitle')}"
        separate-data-choice-title="${this.i18n('separateBrowsingDataChoiceTitle')}"
        separate-data-choice-details="${this.i18n('separateBrowsingDataChoiceDetails')}"
        merge-data-choice-title="${this.i18n('mergeBrowsingDataChoiceTitle')}"
        merge-data-choice-details="${this.i18n('mergeBrowsingDataChoiceDetails')}"
        .selectedDataHandling="${this.selectedDataHandling_}"
        @selected-data-handling-changed="${this.onSelectedDataHandlingChanged_}">
    </managed-user-profile-notice-data-handling>
  ` : ''}
</div>
<div class="action-container">
  <cr-button id="proceedButton"
      aria-label="${this.proceedLabel_}"
      class="action-button tangible-button"
      @click="${this.onProceedButtonClick_}"
      ?disabled="${this.shouldDisableProceedButton_()}"
      ?hidden="${this.isState_(State.PROCESSING)}">
    ${this.proceedLabel_}
  </cr-button>
  <cr-button id="cancelButton"
      aria-label="${this.getCancelLabel_()}"
      class="tangible-button ${this.getCancelButtonClass_()}"
      @click="${this.onCancelButtonClick_}"
      ?hidden="${!this.allowCancel_()}">
    ${this.getCancelLabel_()}
  </cr-button>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
