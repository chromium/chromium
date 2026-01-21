// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {Page, type SkillsAppElement} from './app.js';

export function getHtml(this: SkillsAppElement) {
  return html`
<div role="navigation" id="sidebar">
  <cr-menu-selector id="menu" selectable="a" selected-attribute="selected"
      @iron-select="${this.onMenuItemSelect_}">
    ${this.menuItems_.map(menuItem => html`
        <a role="menuitem" href="${menuItem.page}" class="cr-nav-menu-item"
            @click="${this.onMenuItemClick_}">
          <cr-icon icon="${menuItem.icon}" slot="prefix"></cr-icon>
          <div class="name">${menuItem.name}</div>
        </a>`)}
  </cr-menu-selector>
</div>
<cr-page-selector id="page" attr-for-selected="page-index"
    .selected="${this.selectedPage_}">
  <user-skills-page id="userSkillsPage" page-index="${Page.USER_SKILLS}">
  </user-skills-page>
  <discover-skills-page id="discoverSkillsPage"
      page-index="${Page.DISCOVER_SKILLS}">
  </discover-skills-page>
</cr-page-selector>`;
}
