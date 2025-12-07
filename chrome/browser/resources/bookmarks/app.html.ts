// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {BookmarksAppElement} from './app.js';

export function getHtml(this: BookmarksAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<bookmarks-toolbar sidebar-width="${this.sidebarWidth_}" role="banner">
</bookmarks-toolbar>
<div id="drop-shadow" class="cr-container-shadow"></div>
<div id="main-container">
  <div id="sidebar">
    <div id="sidebar-folders" role="tree" aria-label="$i18n{sidebarAxLabel}">
      <bookmarks-folder-node item-id="0" depth="-1"></bookmarks-folder-node>
    </div>
    <managed-footnote></managed-footnote>
  </div>
  <cr-splitter id="splitter"></cr-splitter>
  <bookmarks-list @scroll="${this.onListScroll_}"></bookmarks-list>
</div>
<bookmarks-command-manager></bookmarks-command-manager>
<cr-toast-manager duration="10000">
  <cr-button @click="${this.onUndoClick_}" aria-label="$i18n{undoDescription}">
    $i18n{undo}
  </cr-button>
</cr-toast-manager>
<!--_html_template_end_-->`;
  // clang-format on
}
