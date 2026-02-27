// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {SettingsItemType, type SettingsMenuElement} from './settings_menu.js';


export function getHtml(this: SettingsMenuElement) {
  // clang-format off
  return html`
<cr-lazy-render-lit id="lazyMenu" .template='${() => html`
  <cr-action-menu id="settings-menu-dialog" @close="${this.onClose_}" non-modal>
    ${this.options_.map((item, index) => html`
      ${item.showSeparator ? html`<hr class="separator" aria-hidden="true">` : ``}
      <button class="menu-row dropdown-item"
          id="${item.id}"
          role="menuitem"
          data-index="${index}"
          title="${item.ariaLabel || item.title}"
          aria-label="${item.ariaLabel || item.title}"
          @pointerenter="${this.onPointerenter_}"
          @pointerleave="${this.onPointerleave_}"
          @click="${this.onMenuItemClick_}">

        <div class="start-container">
          ${item.icon ? html`
            <cr-icon class="start-icon" icon="${item.icon}"></cr-icon>
          ` : ''}

          <div class="label">${item.title}</div>
        </div>

        ${item.itemType === SettingsItemType.TOGGLE ? html`
            <cr-toggle
              title="${item.ariaLabel || item.title}"
              aria-label="${item.ariaLabel || item.title}"
              @click="${this.onMenuItemClick_}"
              ?checked="${item.enabled || false}"
              data-index="${index}">
            </cr-toggle>
        ` : html`
            <cr-icon class="end-icon" icon="cr:chevron-right"></cr-icon>
        `}
      </button>
    `)}
  </cr-action-menu>
`}'>
</cr-lazy-render-lit>
`;
  // clang-format on
}
