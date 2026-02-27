// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {type SkillsSidebarElement} from './sidebar.js';

export function getHtml(this: SkillsSidebarElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-menu-selector selectable="a" selected-attribute="selected"
    @iron-activate="${this.onIronActivate_}" .selected="${this.selectedPage}"
    attr-for-selected="data-path">
  ${this.menuItems.map(menuItem => html`
    <a role="menuitem" href="/${menuItem.page}" class="cr-nav-menu-item"
        @click="${this.onMenuItemClick_}" data-path="${menuItem.page}">
      <cr-icon icon="${menuItem.icon}"></cr-icon>
      <div class="name">${menuItem.name}</div>
    </a>
  `)}
</cr-menu-selector>
<div class="separator"></div>
<div class="footer">
  $i18n{footerText}
  <span class="branding">
    $i18n{footerBranding}
  </span>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
