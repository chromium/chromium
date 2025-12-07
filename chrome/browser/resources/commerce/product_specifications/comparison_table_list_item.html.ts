// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComparisonTableListItemElement} from './comparison_table_list_item.js';

export function getHtml(this: ComparisonTableListItemElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
  <div id="itemContainer">
    <cr-url-list-item
        id="item"
        size="large"
        title="${this.getTitle_()}"
        url="${this.getFaviconUrl_()}"
        description="${this.tableUrl_.url}"
        .imageUrls="${this.imageUrl_ ? [this.imageUrl_?.url] : []}"
        .forceHover="${this.isMenuOpen_}"
        @click="${this.onClick_}"
        @contextmenu="${this.onContextMenu_}"
        always-show-suffix>

      ${this.hasCheckbox ? html`
        <cr-checkbox id="checkbox" slot="prefix"
            @checked-changed="${this.onCheckboxChange_}">
        </cr-checkbox>` : ''}

      ${this.isRenaming_ ?
        html`
          <cr-input slot="content" id="renameInput" value="${this.name}"
              class="stroked" @blur="${this.onRenameInputBlur_}"
              @keydown="${this.onRenameInputKeyDown_}">
          </cr-input>`
        : html`
          <div id="numItems" slot="badges">${this.numItemsString_}</div>
          <!-- Hide the trailing icon button if the item has a checkbox. -->
          ${!this.hasCheckbox ? html`
            <cr-icon-button id="trailingIconButton" slot="suffix"
                iron-icon="cr:more-vert"
                @click="${this.onShowContextMenuClick_}">
            </cr-icon-button>` : ''}`}
    </cr-url-list-item>
  </div>

  <cr-lazy-render-lit id="menu"
      .template="${() => html`
        <cr-action-menu>
          <button id="openInNewTab" class="dropdown-item" role="menuitem"
              @click="${this.onOpenInNewTabClick_}">
            $i18n{menuOpenInNewTab}
          </button>
          <button id="openInNewWindow" class="dropdown-item" role="menuitem"
              @click="${this.onOpenInNewWindowClick_}">
            $i18n{menuOpenInNewWindow}
          </button>
          <hr>
          <!-- Renaming is not available when the checkbox is showing. -->
          <button id="rename" class="dropdown-item" role="menuitem"
              @click="${this.onRenameClick_}" ?disabled="${this.hasCheckbox}">
            $i18n{menuRename}
          </button>
          <button id="delete" class="dropdown-item" role="menuitem"
              @click="${this.onDeleteClick_}">
            $i18n{menuDelete}
          </button>
        </cr-action-menu>`}">
  </cr-lazy-render-lit>
  <!--_html_template_end_-->`;
  // clang-format on
}
