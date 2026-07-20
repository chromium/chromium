// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {MediaMenuElement} from './media_menu.js';

export function getHtml(this: MediaMenuElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-lazy-render-lit id="lazyMenu" .template='${() => html`
  <cr-action-menu id="media-menu-dialog" ?non-modal="${this.nonModal}">
    ${this.options_.map((item, index) => html`
      <button class="toggle-row dropdown-item"
          id="${item.id}-toggle-button"
          role="menuitem"
          title="${item.ariaLabel || item.title}"
          aria-label="${item.ariaLabel || item.title}"
          ?disabled="${!!item.disabled}"
          @click="${this.onToggleItemClick_}"
          data-index="${index}">
        <div class="start-container">
          <cr-icon class="start-icon" icon="${item.icon}"></cr-icon>
          <div class="label">${item.title}</div>
        </div>
        <cr-toggle
            title="${item.ariaLabel || item.title}"
            aria-label="${item.ariaLabel || item.title}"
            ?disabled="${!!item.disabled}"
            @click="${this.onToggleItemClick_}"
            ?checked="${!!item.checked}"
            data-index="${index}">
        </cr-toggle>
      </button>
    `)}
  </cr-action-menu>
`}'>
</cr-lazy-render-lit>
<!--_html_template_end_-->`;
  // clang-format on
}
