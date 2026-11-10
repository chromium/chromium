// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AiFeedbackDialogElement} from './ai_feedback_dialog.js';

export function getHtml(this: AiFeedbackDialogElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-dialog id="aiFeedbackDialog" show-on-attach ignore-popstate
    consume-keydown-event>
  <div slot="title" class="dialog-title-bar">
    <div class="dialog-title">$i18n{aiPlaybackFeedbackDialogTitle}</div>
    <div class="dialog-subtitle">$i18n{aiPlaybackFeedbackDialogSubtitle}</div>
  </div>

  <div slot="body" class="feedback-body" role="group"
      aria-label="$i18n{aiPlaybackFeedbackDialogSubtitle}">
    ${this.reasonOptions_.map(option => html`
      <cr-checkbox class="reason-checkbox" data-reason="${option.id}"
          ?checked="${this.selectedReasons_.includes(option.id)}"
          @checked-changed="${this.onReasonCheckedChanged_}">
        ${option.label}
      </cr-checkbox>
    `)}
  </div>

  <div slot="button-container">
    <cr-button class="cancel-button" id="cancelButton"
        @click="${this.onCancelButtonClick_}">
      $i18n{cancel}
    </cr-button>
    <cr-button class="action-button" id="submitButton"
        ?disabled="${!this.canSubmit_()}" @click="${this.onSubmitClick_}">
      $i18n{aiPlaybackFeedbackSubmit}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
  // clang-format on
}
