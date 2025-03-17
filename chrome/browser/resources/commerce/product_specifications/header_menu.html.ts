// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {HeaderMenuElement} from './header_menu.js';

export function getHtml(this: HeaderMenuElement) {
  return html`<!--_html_template_start_-->
  <cr-lazy-render-lit id="menu" .template="${() => html`
    <cr-action-menu>
      <button id="rename" class="dropdown-item" role="menuitem"
          @click="${this.onRenameClick_}">
        <cr-icon icon="product-specifications:edit"></cr-icon>
        $i18n{renameGroup}
      </button>
      <button id="seeAll" class="dropdown-item" role="menuitem"
          @click="${this.onSeeAllClick_}">
        <cr-icon icon="product-specifications:see-all"></cr-icon>
        $i18n{seeAll}
      </button>
      <button id="delete" class="dropdown-item" role="menuitem"
          @click="${this.onDeleteClick_}">
        <cr-icon icon="product-specifications:delete"></cr-icon>
        $i18n{delete}
      </button>
    </cr-action-menu>
  `}">
  </cr-lazy-render-lit>
  <!--_html_template_end_-->`;
}
