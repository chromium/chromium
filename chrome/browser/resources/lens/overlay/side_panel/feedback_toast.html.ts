// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {FeedbackToastElement} from './feedback_toast.js';

export function getHtml(this: FeedbackToastElement) {
  return html`
<cr-toast id="feedbackToast" duration="0">
  <div id="feedbackToastMessage">${
      loadTimeData.getString('feedbackToastMessage')}</div>
  <div id="feedbackButtonContainer">
    <cr-button id="sendFeedbackButton" @click="${this.onSendFeedbackClick}">
      ${loadTimeData.getString('sendFeedbackButtonText')}
    </cr-button>
    <cr-icon-button id="closeFeedbackToastButton"
        aria-label="${
      loadTimeData.getString('closeFeedbackToastAccessibilityLabel')}"
        iron-icon="cr:close" @click="${this.onHideFeedbackToastClick}">
    </cr-icon-button>
  </div>
</cr-toast>`;
}
