// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {DisplayItem, PowerBookmarksListElement} from './power_bookmarks_list.js';

export function getHtml(this: PowerBookmarksListElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="scroller" class="sp-scroller" scrollable role="list">
  <div class="sp-card">
    <power-bookmarks-list-header id="heading"
        ?hidden="${!this.sectionVisibility_.heading}"
        .activeFolder="${this.getActiveFolder()}"
        .compact="${this.compact_}" .disableEdit="${!this.hasShownBookmarks}"
        .editing="${this.editing}"
        @sort-changed="${this.onSortChanged_}"
        @back-clicked="${this.onBackClicked_}"
        @view-toggled="${this.onViewToggled_}">
    </power-bookmarks-list-header>

    ${!this.sectionVisibility_.bookmarksList ? html`
      <div ?hidden="${!this.sectionVisibility_.newFolderButton}">
        <power-bookmarks-add-folder-button
            ?disabled="${this.editing}"
            ?compact="${this.compact_}"
            @click="${this.onAddNewFolderClick_}">
        </power-bookmarks-add-folder-button>
      </div>
    ` : ''}

    <sp-empty-state id="folderEmptyState"
        ?hidden="${!this.sectionVisibility_.folderEmptyState}"
        image-path="./images/bookmarks_empty.svg"
        dark-image-path="./images/bookmarks_empty_dark.svg"
        heading="$i18n{emptyTitleFolder}"
        body="$i18n{emptyBodyFolder}">
    </sp-empty-state>

    <div id="bookmarks" class="bookmarks"
        ?hidden="${!this.sectionVisibility_.bookmarksList}"
        role="${this.getBookmarksListRole_()}"
        aria-multiselectable="${this.editing}" scrollable>
      <power-bookmarks-add-folder-button
          ?hidden="${!this.sectionVisibility_.newFolderButton}"
          ?disabled="${this.editing}"
          ?compact="${this.compact_}"
          @click="${this.onAddNewFolderClick_}">
      </power-bookmarks-add-folder-button>
      <cr-lazy-list id="list"
          .items="${this.displayList_}" .scrollTarget="${this.scrollTarget_}"
          role="tree"
          .template="${(item: DisplayItem, index: number) => html`
            <power-bookmark-row id="bookmark-${item.bookmark.id}"
                .bookmark="${item.bookmark}" .depth="${item.depth}"
                .compact="${this.compact_}"
                .searchQuery="${this.searchQuery}"
                .updatedElementIds="${this.updatedElementIds_}"
                trailing-icon-tooltip="$i18n{tooltipMore}"
                .hasCheckbox="${this.editing}"
                .activeSortIndex="${this.activeSortIndex}"
                .selectedBookmarks=
                    "${this.getSelectedBookmarksList_()}"
                .renamingId="${this.renamingId}"
                @row-clicked="${this.onRowClicked_}"
                @context-menu="${this.onShowContextMenu_}"
                @trailing-icon-clicked="${this.onTrailingIconClicked_}"
                @checkbox-change="${this.onCheckboxChange_}"
                @input-change="${this.onInputChange_}"
                @list-item-size-changed="${this.onListItemSizeChanged_}"
                @power-bookmark-toggle="${this.onPowerBookmarkToggle_}"
                @power-bookmark-row-focus-parent="${this.onPowerBookmarkRowFocusParent_}"
                tabindex="${this.tabIndex}"
                .imageUrls="${this.imageUrls_}"
                .shoppingCollectionFolderId="${this.shoppingCollectionFolderId_}"
                .contextMenuBookmark="${this.contextMenuBookmark}"
                draggable="${this.canDrag_}"
                .canDrag="${this.canDrag_}"
                .hasActiveDrag="${this.hasActiveDrag_}"
                .activeFolderPath="${this.activeFolderPath}"
                .hasFolders="${this.hasFolders_}"
                .rowHeading="${this.getRowHeading_(index)}">
            </power-bookmark-row>`}">
      </cr-lazy-list>
    </div>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
