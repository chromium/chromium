// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {type SkillsSidebarElement} from './sidebar.js';

export function getHtml(this: SkillsSidebarElement) {
  // clang-format off
  return html`
<cr-menu-selector selectable="a" selected-attribute="selected"
    @iron-select="${this.onMenuItemSelect_}" .selected="${this.selectedPage}"
    attr-for-selected="href">
  ${this.menuItems.map(menuItem => html`
    <a role="menuitem" href="${menuItem.page}" class="cr-nav-menu-item"
        @click="${this.onMenuItemClick_}">
      <cr-icon icon="${menuItem.icon}"></cr-icon>
      <div class="name">${menuItem.name}</div>
    </a>
  `)}
</cr-menu-selector>`;
  // clang-format on
}
