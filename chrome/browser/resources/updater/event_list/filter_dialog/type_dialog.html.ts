// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TypeDialogElement} from './type_dialog.js';

export function getHtml(this: TypeDialogElement) {
  // clang-format off
  return html`
<!--_html_template_start_-->
<filter-dialog .anchorElement="${this.anchorElement}" @close="${this.onClose}">
  ${this.filterMenuItems.map(item => html`
    <button class="filter-menu-item"
        data-filter-category="${item.filterCategory}"
        @click="${this.onClick}">
      ${item.label}
    </button>
  `)}
</filter-dialog>
<!--_html_template_end_-->`;
  // clang-format on
}
