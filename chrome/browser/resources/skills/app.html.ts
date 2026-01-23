// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {Page, type SkillsAppElement} from './app.js';

// TODO(b/475607224): Instead of hardcoding, add resource strings for
// labels and names.
export function getHtml(this: SkillsAppElement) {
  // clang-format off
  return html`
<cr-toolbar id="toolbar" page-name="Skills" clear-label="Delete"
    ?autofocus="${true}" search-prompt="Search skills"
    @cr-toolbar-menu-click="${this.onMenuButtonClick_}"
    menu-label="Main menu" @search-changed="${this.onSearchChanged_}"
    role="banner" .narrow="${this.narrow_}"
    @narrow-changed="${this.onNarrowChanged_}" narrow-threshold="980"
    ?show-menu="${this.narrow_}">
</cr-toolbar>

<div id="content" class="no-outline cr-scrollable">
  <div id="left">
    <div role="navigation" id="sidebar">
      <cr-menu-selector id="menu" selectable="a" selected-attribute="selected"
          @iron-select="${this.onMenuItemSelect_}">
        ${this.menuItems_.map(menuItem => html`
          <a role="menuitem" href="${menuItem.page}" class="cr-nav-menu-item"
              @click="${this.onMenuItemClick_}">
            <cr-icon icon="${menuItem.icon}"></cr-icon>
            <div class="name">${menuItem.name}</div>
          </a>
        `)}
      </cr-menu-selector>
    </div>
  </div>
<cr-page-selector id="page" attr-for-selected="page-index"
    .selected="${this.selectedPage_}">
  <user-skills-page id="userSkillsPage" page-index="${Page.USER_SKILLS}">
  </user-skills-page>
  <discover-skills-page id="discoverSkillsPage"
      page-index="${Page.DISCOVER_SKILLS}">
  </discover-skills-page>
</cr-page-selector>`;
  // clang-format on
}
