// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrLazyListElement} from '//resources/cr_elements/cr_lazy_list/cr_lazy_list.js';
import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {DisplayItem, PowerBookmarksListElement} from './power_bookmarks_list.js';

export interface TemplatizedDomNodes {
  listA: CrLazyListElement<DisplayItem>;
  listB: CrLazyListElement<DisplayItem>;
}

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


    <div id="bookmarks" class="bookmarks"
        role="${this.getBookmarksListRole_()}"
        aria-multiselectable="${this.editing}" scrollable>
      <power-bookmarks-add-folder-button
          ?hidden="${!this.sectionVisibility_.newFolderButton}"
          ?disabled="${this.editing}"
          ?compact="${this.compact_}"
          @click="${this.onAddNewFolderClick_}">
      </power-bookmarks-add-folder-button>
      <div id="list-container">
        <div id="list-a" class="${this.getListClass_('a')}">
          <sp-empty-state id="folderEmptyStateA"
              ?hidden="${!this.isFolderEmptyStateVisible_('a')}"
              image-path="./images/bookmarks_empty.svg"
              dark-image-path="./images/bookmarks_empty_dark.svg"
              heading="$i18n{emptyTitleFolder}"
              body="$i18n{emptyBodyFolder}">
          </sp-empty-state>
          <cr-lazy-list id="listA"
              .items="${this.displayListA_}"
              .scrollTarget="${this.scrollTarget_}"
              .itemSize="${this.getItemSize_()}"
              role="tree"
              .template="${(item: DisplayItem, index: number) => html`
                <power-bookmark-row
                    id="bookmark-${item.bookmark.id}"
                    ?hidden="${!this.isListActiveOrTransitioning_('a')}"
                    .bookmark="${item.bookmark}"
                    .depth="${item.depth}"
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
                    @power-bookmark-row-focus-parent=
                        "${this.onPowerBookmarkRowFocusParent_}"
                    tabindex="${this.tabIndex}"
                    .imageUrls="${this.imageUrls_}"
                    .shoppingCollectionFolderId=
                        "${this.shoppingCollectionFolderId_}"
                    .contextMenuBookmark="${this.contextMenuBookmark}"
                    draggable="${this.canDrag_}"
                    .canDrag="${this.canDrag_}"
                    .hasActiveDrag="${this.hasActiveDrag_}"
                    .activeFolderPath="${this.activeFolderPath}"
                    .hasFolders="${this.hasFoldersA_}"
                    .rowHeading="${this.getRowHeading_(index, 'a')}"
                    .toggleExpand="${this.expandedFolderIds_.has(
                        item.bookmark.id)}">
                </power-bookmark-row>`}">
          </cr-lazy-list>
        </div>
        <div id="list-b" class="${this.getListClass_('b')}">
          <sp-empty-state id="folderEmptyStateB"
              ?hidden="${!this.isFolderEmptyStateVisible_('b')}"
              image-path="./images/bookmarks_empty.svg"
              dark-image-path="./images/bookmarks_empty_dark.svg"
              heading="$i18n{emptyTitleFolder}"
              body="$i18n{emptyBodyFolder}">
          </sp-empty-state>
          <cr-lazy-list id="listB"
              .items="${this.displayListB_}"
              .scrollTarget="${this.scrollTarget_}"
              .itemSize="${this.getItemSize_()}"
              role="tree"
              .template="${(item: DisplayItem, index: number) => html`
                <power-bookmark-row
                    id="bookmark-${item.bookmark.id}"
                    ?hidden="${!this.isListActiveOrTransitioning_('b')}"
                    .bookmark="${item.bookmark}"
                    .depth="${item.depth}"
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
                    @power-bookmark-row-focus-parent=
                        "${this.onPowerBookmarkRowFocusParent_}"
                    tabindex="${this.tabIndex}"
                    .imageUrls="${this.imageUrls_}"
                    .shoppingCollectionFolderId=
                        "${this.shoppingCollectionFolderId_}"
                    .contextMenuBookmark="${this.contextMenuBookmark}"
                    draggable="${this.canDrag_}"
                    .canDrag="${this.canDrag_}"
                    .hasActiveDrag="${this.hasActiveDrag_}"
                    .activeFolderPath="${this.activeFolderPath}"
                    .hasFolders="${this.hasFoldersB_}"
                    .rowHeading="${this.getRowHeading_(index, 'b')}"
                    .toggleExpand="${this.expandedFolderIds_.has(
                        item.bookmark.id)}">
                </power-bookmark-row>`}">
          </cr-lazy-list>
        </div>
      </div>
    </div>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
