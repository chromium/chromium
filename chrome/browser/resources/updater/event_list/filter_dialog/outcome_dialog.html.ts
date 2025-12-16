// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {COMMON_UPDATE_OUTCOMES, localizeUpdateOutcome} from '../../event_history.js';
import type {CommonUpdateOutcome} from '../../event_history.js';

import type {OutcomeDialogElement} from './outcome_dialog.js';

export function getHtml(this: OutcomeDialogElement) {
  // clang-format off
  return html`
<!--_html_template_start_-->
<filter-dialog .anchorElement="${this.anchorElement}"
    @close="${this.onClose}">
  ${COMMON_UPDATE_OUTCOMES.map((item: CommonUpdateOutcome) => html`
    <cr-checkbox class="filter-menu-item"
        ?checked="${this.pendingSelections.has(item)}"
        data-outcome="${item}"
        @checked-changed="${this.onCheckedChanged}">
      ${localizeUpdateOutcome(item)}
    </cr-checkbox>
  `)}
  <filter-dialog-footer
      @cancel-click="${this.onCancelClick}"
      @apply-click="${this.onApplyClick}">
  </filter-dialog-footer>
</filter-dialog>
<!--_html_template_end_-->`;
  // clang-format on
}
