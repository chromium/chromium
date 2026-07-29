// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {GroupedActionMenuElement} from './grouped_action_menu.js';

export function getHtml(this: GroupedActionMenuElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-lazy-render-lit  id="lazyMenu" .template='${() => html`
  <cr-action-menu
      accessibility-label="${this.label}"
      role-description="$i18n{menu}"
      ?non-modal="${this.nonModal}"
      tabindex="-1">
    ${this.menuGroups.map((group, groupIndex) => html`
      <span class="menu-group" role="group" aria-label="${group.header.title}"
          aria-owns="${this.getAriaOwns_(groupIndex, group.items.length)}">
      </span>
      <hr class="sp-hr has-separator-${group.header.separator}">
      <span
          class="header-style"
          role="heading">
          ${group.header.title}
      </span>
      ${group.items.map((item, itemIndex) => html`
        <button
            id="group-${groupIndex}-item-${itemIndex}"
            class="${this.getItemClass_(item)}"
            style="${item.style}"
            role="${this.getItemRole_(item)}"
            aria-label="${item.ariaLabel}"
            aria-checked="${this.getItemAriaChecked_(item)}"
            @click="${this.onClick_}"
            data-group-index="${groupIndex}"
            data-item-index="${itemIndex}">
          <cr-icon
              class="${this.getItemIconClass_(item)}"
              icon="${this.getItemIcon_(item)}">
          </cr-icon>
          <cr-icon
              class="button-image has-icon-${!!item.icon}"
              icon="${item.icon || ''}">
          </cr-icon>
          ${item.title}
        </button>
      `)}
    `)}
  </cr-action-menu>
`}'>
</cr-lazy-render-lit>
<!--_html_template_end_-->`;
  // clang-format on
}
