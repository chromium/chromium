// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {FeedbackToastElement} from './feedback_toast.js';

export function getHtml(this: FeedbackToastElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-toast id="feedbackToast" duration="0">
  <div id="feedbackToastMessage">$i18n{feedbackToastMessage}</div>
  <div id="feedbackButtonContainer">
    <cr-button id="sendFeedbackButton" @click="${this.onSendFeedbackClick}">
      $i18n{sendFeedbackButtonText}
    </cr-button>
    <cr-icon-button id="closeFeedbackToastButton"
        aria-label="$i18n{closeFeedbackToastAccessibilityLabel}"
        iron-icon="cr:close" @click="${this.onHideFeedbackToastClick}">
    </cr-icon-button>
  </div>
</cr-toast>
<!--_html_template_end_-->`;
  // clang-format on
}
