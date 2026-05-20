/* Copyright 2026 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

import '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PowerBookmarksAppElement} from './power_bookmarks_app.js';

export function getHtml(this: PowerBookmarksAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="column" id="powerBookmarksContainer">
  <cr-toolbar-search-field id="searchField" label="$i18n{searchBookmarks}"
      clear-label="$i18n{clearSearch}"
      @search-changed="${this.onSearchChanged_}"
      ?disabled="${this.editing_}" ?hidden="${!this.sectionVisibility_.search}"
      @blur="${this.onSearchBlur_}">
  </cr-toolbar-search-field>
  <div class="label-row" ?hidden="${!this.sectionVisibility_.labels}"
      ?header-hidden="${!this.sectionVisibility_.heading}" id="labelsContainer">
    <power-bookmarks-labels id="labels" ?disabled="${this.editing_}"
        .trackedProductInfos="${this.trackedProductInfos_}"
        @labels-changed="${this.onLabelsChanged_}">
    </power-bookmarks-labels>
  </div>

  <power-bookmarks-list id="bookmarksList"
      ?hidden="${!!this.sectionVisibility_.topLevelEmptyState}"
      .activeFolderPath="${this.activeFolderPath_}"
      @active-folder-path-changed="${this.onActiveFolderPathChanged_}"
      @has-shown-bookmarks-changed="${this.onHasShownBookmarksChanged_}"
      .labels="${this.labels_}"
      .selectedBookmarks="${this.selectedBookmarks_}"
      .contextMenuBookmark="${this.contextMenuBookmark_}"
      .hasSomeActiveFilter="${this.hasSomeActiveFilter_}"
      .searchQuery="${this.searchQuery_}"
      .editing="${this.editing_}"
      .renamingId="${this.renamingId_}"
      @renaming-id-changed="${this.onRenamingIdChanged_}"
      @has-scrollbars-changed="${this.onHasScrollbarsChanged_}"
      @row-selected-change="${this.onRowSelectedChange_}"
      @bulk-edit="${this.onBookmarksListBulkEdit_}"
      @clear-search="${this.onClearSearch_}"
      @show-context-menu="${this.onShowContextMenu_}"
      @disabled-feature="${this.onDisabledFeature_}">
  </power-bookmarks-list>

  <div class="sp-scroller"
      ?hidden="${!this.sectionVisibility_.topLevelEmptyState}">
    <sp-empty-state id="topLevelEmptyState"
        ?guest="${this.guestMode_}"
        image-path="${this.getEmptyImagePath_()}"
        dark-image-path="${this.getEmptyImagePathDark_()}"
        heading="${this.getEmptyTitle_()}"
        body="${this.getEmptyBody_()}">
    </sp-empty-state>
  </div>
  <sp-footer id="footer"
      ?hidden="${!this.sectionVisibility_.footer}"
      pinned>
    <cr-button id="addCurrentTabButton"
        class="floating-button"
        ?hidden="${this.hideAddTabButton_()}"
        @click="${this.onAddTabClick_}"
        ?disabled="${!this.canAddCurrentUrl_}">
      <cr-icon slot="prefix-icon" icon="sp:add-circle"></cr-icon>
      $i18n{addCurrentTab}
    </cr-button>

    <cr-toolbar-selection-overlay ?show="${this.editing_}"
        cancel-label="$i18n{cancelA11yLabel}"
        selection-label="${this.getSelectedDescription_()}"
        @clear-selected-items="${this.onClearSelectedItems_}">
      <div class="sp-icon-buttons-row">
        <cr-icon-button id="deleteButton" iron-icon="bookmarks:delete"
            ?disabled="${!this.getSelectedBookmarksLength_()}"
            title="$i18n{tooltipDelete}" aria-label="$i18n{tooltipDelete}"
            @click="${this.onDeleteClick_}">
        </cr-icon-button>
        <cr-icon-button iron-icon="bookmarks:move"
            ?disabled="${!this.getSelectedBookmarksLength_()}"
            title="$i18n{tooltipMove}" aria-label="$i18n{tooltipMove}"
            @click="${this.onMoveClick_}">
        </cr-icon-button>
        <cr-icon-button iron-icon="cr:more-vert"
            ?disabled="${!this.getSelectedBookmarksLength_()}"
            title="$i18n{tooltipMore}" aria-label="$i18n{tooltipMore}"
            @click="${this.onBulkEditMenuClick_}">
        </cr-icon-button>
      </div>
    </cr-toolbar-selection-overlay>
  </sp-footer>
</div>

<power-bookmarks-edit-dialog id="editDialog"
    @save="${this.onEditDialogSave_}">
</power-bookmarks-edit-dialog>

<cr-dialog id="disabledFeatureDialog">
  <div slot="body" class="dialog-body">
    <cr-icon icon="cr:domain"></cr-icon>
    <div>$i18n{disabledFeature}</div>
  </div>
  <div class="button-row" slot="button-container">
    <cr-button @click="${this.onCloseDisabledFeatureDialogClick_}">
      $i18n{ok}
    </cr-button>
  </div>
</cr-dialog>

<power-bookmarks-context-menu id="contextMenu"
    @delete-clicked="${this.onContextMenuDeleteClicked_}"
    @disabled-feature="${this.onDisabledFeature_}"
    @edit-clicked="${this.onContextMenuEditClicked_}"
    @rename-clicked="${this.onContextMenuRenameClicked_}"
    @close="${this.onContextMenuClose_}">
</power-bookmarks-context-menu>

<cr-lazy-render-lit id="deletionToast" .template="${() => html`
  <cr-toast duration="5000">
    <div>${this.deletionDescription_}</div>
    <cr-button @click="${this.onUndoClick_}">
      $i18n{undoBookmarkDeletion}
    </cr-button>
  </cr-toast>
`}">
</cr-lazy-render-lit>
<!--_html_template_end_-->`;
  // clang-format on
}
