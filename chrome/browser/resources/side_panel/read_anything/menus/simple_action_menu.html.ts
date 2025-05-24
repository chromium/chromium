// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SimpleActionMenuElement} from './simple_action_menu.js';

export function getHtml(this: SimpleActionMenuElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-lazy-render-lit  id="lazyMenu" .template='${() => html`
  <cr-action-menu
      accessibility-label="${this.label}"
      role-description="$i18n{menu}"
      tabindex="-1">
    ${this.menuItems.map((item, index) => html`
      <button
          class="dropdown-item"
          @click="${this.onClick_}"
          data-index="${index}">
        <cr-icon
            class="button-image check-mark check-mark-showing-${this.isItemSelected_(index)}"
            icon="read-anything-20:check-mark"
            aria-label="$i18n{selected}">
        </cr-icon>
        <cr-icon
            class="button-image has-icon-${this.doesItemHaveIcon_(item)}"
            icon="${this.itemIcon_(item)}">
        </cr-icon>
        ${item.title}
      </button>
    `)}
  </cr-action-menu>
`}'>
</cr-lazy-render-lit>
<!--_html_template_end_-->`;
  // clang-format on
}
