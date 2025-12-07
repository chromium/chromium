// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {BookmarksFolderNodeElement} from './folder_node.js';

export function getHtml(this: BookmarksFolderNodeElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="container"
    class="cr-vertical-tab cr-nav-menu-item ${this.getContainerClass_()}"
    ?hidden="${this.isRootFolder_()}"
    role="treeitem"
    aria-level="${this.getAriaLevel_()}"
    aria-owns="descendants"
    tabindex="${this.getTabIndex_()}"
    @click="${this.selectFolder_}"
    @dblclick="${this.toggleFolder_}"
    @contextmenu="${this.onContextMenu_}"
    ?selected="${this.isSelectedFolder_}"
    aria-selected="${this.isSelectedFolder_}">
  <div id="inner-container">
    ${this.hasChildFolder_ ? html`
      <cr-icon-button id="arrow" iron-icon="cr:arrow-drop-down"
          @click="${this.toggleFolder_}" @mousedown="${this.preventDefault_}"
          tabindex="-1" ?is-open="${this.isOpen}" noink aria-hidden="true">
      </cr-icon-button>` : ''}
    <div class="folder-icon icon-folder-open"
        ?open="${this.isSelectedFolder_}"
        ?no-children="${!this.hasChildFolder_}">
    </div>
    <div class="menu-label" title="${this.getItemTitle_()}">
      ${this.getItemTitle_()}
    </div>
    <cr-ripple></cr-ripple>
  </div>
</div>
<div id="descendants" role="group">
  ${this.isOpen ? html`
    ${this.getFolderChildren_().map(child => html`
      <bookmarks-folder-node item-id="${child}"
          draggable="true" depth="${this.getChildDepth_()}">
      </bookmarks-folder-node>`)}`
   : ''}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
