// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AppDialogElement} from './app_dialog.js';

export function getHtml(this: AppDialogElement) {
  // clang-format off
  return html`
<!--_html_template_start_-->
<filter-dialog .anchorElement="${this.anchorElement}"
    @close="${this.onClose}">
  <input type="text" class="filter-menu-input"
      placeholder="$i18n{appNameOrId}" .value="${this.search}"
      @input="${this.onAppSearchInput}"
      @keydown="${this.onAppSearchKeydown}">
  ${this.displayedApps.map(item => html`
    <cr-checkbox class="filter-menu-item"
        ?checked="${this.pendingSelections.has(item)}"
        data-app-name="${item}" @checked-changed="${this.onCheckedChanged}">
      ${item}
    </cr-checkbox>
  `)}
  <filter-dialog-footer
      @cancel-click="${this.onCancelClick}"
      @apply-click="${this.onAppApplyClick}">
  </filter-dialog-footer>
</filter-dialog>
<!--_html_template_end_-->`;
  // clang-format on
}
