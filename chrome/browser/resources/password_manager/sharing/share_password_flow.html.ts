// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {ShareFlowState} from './share_password_flow.js';
import type {SharePasswordFlowElement} from './share_password_flow.js';

export function getHtml(this: SharePasswordFlowElement) {
  return html`<!--_html_template_start_-->
${this.isState_(ShareFlowState.FETCHING) ? html`
  <share-password-loading-dialog @close="${this.onDialogClose_}"
      dialog-title="${this.getShareDialogTitle_()}">
  </share-password-loading-dialog>
` : ''}

${this.isState_(ShareFlowState.ERROR) ? html`
  <share-password-error-dialog @cancel="${this.onDialogCancel_}"
      @restart="${this.onRestart_}">
  </share-password-error-dialog>
` : ''}

${this.isState_(ShareFlowState.NO_OTHER_MEMBERS) ? html`
  <share-password-no-other-family-members-dialog @close="${this.onDialogClose_}"
      dialog-title="${this.getShareDialogTitle_()}">
  </share-password-no-other-family-members-dialog>
` : ''}

${this.isState_(ShareFlowState.NOT_FAMILY_MEMBER) ? html`
  <share-password-not-family-member-dialog @close="${this.onDialogClose_}"
      dialog-title="${this.getShareDialogTitle_()}">
  </share-password-not-family-member-dialog>
` : ''}

${this.isState_(ShareFlowState.FAMILY_PICKER) ? html`
  <share-password-family-picker-dialog @close="${this.onDialogClose_}"
      dialog-title="${this.getShareDialogTitle_()}"
      .members="${this.fetchResults_!.familyMembers}"
      .selectedRecipients="${this.recipients_}"
      @selected-recipients-changed="${this.onSelectedRecipientsChanged_}"
      @start-share="${this.onStartShare_}">
  </share-password-family-picker-dialog>
` : ''}

${this.isState_(ShareFlowState.CONFIRMATION) ? html`
  <share-password-confirmation-dialog @close="${this.onDialogClose_}"
      .recipients="${this.recipients_}" .password="${this.password}"
      password-name="${this.passwordName}" icon-url="${this.iconUrl}">
  </share-password-confirmation-dialog>
` : ''}
<!--_html_template_end_-->`;
}
