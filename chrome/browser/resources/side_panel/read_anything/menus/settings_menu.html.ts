// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {SettingsOption} from '../content/read_anything_types.js';

import {SettingsItemType, type SettingsMenuElement} from './settings_menu.js';


export function getHtml(this: SettingsMenuElement) {
  // clang-format off
  return html`
<cr-lazy-render-lit id="lazyMenu" .template='${() => html`
  <cr-action-menu id="settings-menu-dialog" non-modal>
    ${this.options_.map((item, index) => html`
      <button class="menu-row ${item.className || ''}"
          role="menuitem"
          data-index="${index}"
          @click="${this.onMenuItemClick_}">


        ${item.icon ? html`
          <cr-icon class="start-icon" icon="${item.icon}"></cr-icon>
        ` : ''}

        <div class="label">${item.ariaLabel}</div>

        <!-- TODO(crbug.com/471212662): Add a designated toggle menu and delete
        this SettingsOption.VIEW check -->
        ${item.itemType === SettingsItemType.TOGGLE ? html`
            <cr-toggle></cr-toggle>
        ` : item.id === SettingsOption.VIEW ? '' : html`
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
