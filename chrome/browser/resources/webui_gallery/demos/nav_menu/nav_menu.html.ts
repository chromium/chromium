// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {NavMenuElement} from './nav_menu.js';

export function getHtml(this: NavMenuElement) {
  return html`
<cr-menu-selector id="selector" selectable="a" selected-attribute="selected"
    selected="${this.selectedIndex}"
    @selected-changed="${this.onSelectedIndexChanged_}"
    @click="${this.onSelectorClick_}">
  ${this.menuItems_.map(item => html`
    <a role="menuitem" href="${item.path}" class="cr-nav-menu-item">
      <cr-icon icon="${item.icon}" ?hidden="${!this.showIcons}"></cr-icon>
      ${item.name}
      ${this.showRipples ? html`
        <cr-ripple></cr-ripple>
      ` : ''}
    </a>
  `)}
</cr-menu-selector>`;
}
