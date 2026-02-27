// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import '/strings.m.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {type SkillsAppElement} from './app.js';
import {ErrorType} from './error_page.js';
import {Page} from './sidebar.js';

export function getHtml(this: SkillsAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-toolbar id="toolbar" page-name="$i18n{skillsTitle}"
    clear-label="$i18n{delete}" ?autofocus="${true}"
    search-prompt="$i18n{searchBarPlaceholderText}"
    @cr-toolbar-menu-click="${this.onCrToolbarMenuClick_}"
    menu-label="$i18n{mainMenu}" @search-changed="${this.onSearchChanged_}"
    role="banner" .narrow="${this.narrow_}"
    @narrow-changed="${this.onNarrowChanged_}" narrow-threshold="980"
    ?show-menu="${this.shouldShowToolbarMenu_()}"
    .showSearch="${!this.shouldShowErrorPage_}">
</cr-toolbar>
${this.shouldShowErrorPage_ ? html`
  <error-page error-type="${ErrorType.GLIC_NOT_ENABLED}"></error-page>` : html`
  <div id="content" class="no-outline cr-scrollable">
    <div id="left">
      <div role="navigation" id="sidebar" ?hidden="${this.narrow_}">
        <skills-sidebar id="menu" .selectedPage="${this.selectedPage_}">
        </skills-sidebar>
      </div>
      <cr-drawer id="drawer" heading="$i18n{skillsTitle}"
          @close="${this.onDrawerClose_}">
        <skills-sidebar id="drawerMenu" slot="body"
            .selectedPage="${this.selectedPage_}">
        </skills-sidebar>
      </cr-drawer>
    </div>
    <cr-page-selector id="page" attr-for-selected="page-index"
        .selected="${this.selectedPage_}">
      <user-skills-page id="userSkillsPage" page-index="${Page.USER_SKILLS}">
      </user-skills-page>
      <discover-skills-page id="discoverSkillsPage"
          page-index="${Page.DISCOVER_SKILLS}">
      </discover-skills-page>
    </cr-page-selector>
  </div>
`}
<!--_html_template_end_-->`;
  // clang-format on
}
