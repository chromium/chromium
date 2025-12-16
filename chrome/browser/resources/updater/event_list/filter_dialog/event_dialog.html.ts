// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {localizeEventType} from '../../event_history.js';
import type {EventType} from '../../event_history.js';

import type {EventDialogElement} from './event_dialog.js';

export function getHtml(this: EventDialogElement) {
  // clang-format off
  return html`
<!--_html_template_start_-->
<filter-dialog .anchorElement="${this.anchorElement}"
    @close="${this.onClose}">
  <div class="filter-menu-section-header">$i18n{common}</div>
  ${this.commonEventTypes.map(item => html`
    <cr-checkbox class="filter-menu-item"
        ?checked="${this.pendingSelections.has(item)}"
        data-event-type="${item}" @checked-changed="${this.onCheckedChanged}">
      ${localizeEventType(item)}
    </cr-checkbox>
  `)}
  <div class="filter-menu-section-header">$i18n{other}</div>
  ${this.otherEventTypes.map((item: EventType) => html`
    <cr-checkbox class="filter-menu-item"
        ?checked="${this.pendingSelections.has(item)}"
        data-event-type="${item}" @checked-changed="${this.onCheckedChanged}">
      ${localizeEventType(item)}
    </cr-checkbox>
  `)}
  <filter-dialog-footer
      @cancel-click="${this.onCancelClick}" @apply-click="${this.onApplyClick}">
  </filter-dialog-footer>
</filter-dialog>
<!--_html_template_end_-->`;
  // clang-format on
}
