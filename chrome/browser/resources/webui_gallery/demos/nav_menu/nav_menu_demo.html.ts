// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {NavMenuDemoElement} from './nav_menu_demo.js';

export function getHtml(this: NavMenuDemoElement) {
  return html`
<h1>Navigation menus</h1>
<div class="demos">
  <cr-checkbox ?checked="${this.showIcons_}"
      @checked-changed="${this.onShowIconsChanged_}">
    Show icons
  </cr-checkbox>
  <cr-checkbox ?checked="${this.showRipples_}"
      @checked-changed="${this.onShowRipplesChanged_}">
    Show ripples on click
  </cr-checkbox>
  <cr-button @click="${this.showDrawerMenu_}">Show menu in a drawer</cr-button>
  <nav-menu ?hidden="${this.isDrawerOpen_}"
      ?show-icons="${this.showIcons_}" ?show-ripples="${this.showRipples_}"
      .selectedIndex="${this.selectedIndex_}"
      @selected-index-changed="${this.onSelectedIndexChanged_}">
  </nav-menu>
  <div>Selected index: ${this.selectedIndex_}</div>
</div>

<cr-drawer id="drawer" heading="Drawer" @close="${this.onDrawerClose_}">
  <div slot="body">
    <nav-menu
        ?show-icons="${this.showIcons_}" ?show-ripples="${this.showRipples_}"
        .selectedIndex="${this.selectedIndex_}"
        @selected-index-changed="${this.onSelectedIndexChanged_}">
    </nav-menu>
  </div>
</cr-drawer>`;
}
