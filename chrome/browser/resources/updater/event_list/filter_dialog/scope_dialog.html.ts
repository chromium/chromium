// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {localizeScope, SCOPES} from '../../event_history.js';

import type {ScopeDialogElement} from './scope_dialog.js';

export function getHtml(this: ScopeDialogElement) {
  return html`
<!--_html_template_start_-->
<filter-dialog .anchorElement="${this.anchorElement}" @close="${this.onClose}">
  ${SCOPES.map(scope => html`
    <cr-checkbox class="filter-menu-item"
        ?checked="${this.pendingSelections.has(scope)}"
        data-scope="${scope}" @checked-changed="${this.onCheckedChanged}">
      ${localizeScope(scope)}
    </cr-checkbox>
  `)}
  <filter-dialog-footer @apply-click="${this.onApplyClick}"
      @cancel-click="${this.onCancelClick}">
  </filter-dialog-footer>
</filter-dialog>
<!--_html_template_end_-->`;
}
