// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {DateDialogElement} from './date_dialog.js';

export function getHtml(this: DateDialogElement) {
  // clang-format off
  return html`
<!--_html_template_start_-->
<filter-dialog .anchorElement="${this.anchorElement}"
    @close="${this.onClose}">
  <div class="filter-menu-date-inputs">
    <label for="start-date">$i18n{startDate}</label>
    <input type="datetime-local" id="start-date"
        .valueAsNumber="${this.pendingStartTime}"
        @input="${this.onStartTimeInput}">
    <label for="end-date">$i18n{endDate}</label>
    <input type="datetime-local" id="end-date"
        .valueAsNumber="${this.pendingEndTime}"
        @input="${this.onEndTimeInput}">
  </div>
  <filter-dialog-footer
      @cancel-click="${this.onCancelClick}"
      @apply-click="${this.onDateApplyClick}">
  </filter-dialog-footer>
</filter-dialog>
<!--_html_template_end_-->`;
  // clang-format on
}
