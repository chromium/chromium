// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {RelatedWebsiteSetsSidebarElement} from './sidebar.js';

export function getHtml(this: RelatedWebsiteSetsSidebarElement) {
  return html`
  <cr-menu-selector id="selector" selectable="a" selected-attribute="selected">
    ${this.menuItems_.map(item => html`
      <a role="menuitem" href="${item.path}" class="cr-nav-menu-item">
        <cr-icon class="icon" icon="${item.icon}"></cr-icon>
        ${item.name}
        <cr-ripple></cr-ripple>
      </a>
    `)}
  </cr-menu-selector>`;
}
