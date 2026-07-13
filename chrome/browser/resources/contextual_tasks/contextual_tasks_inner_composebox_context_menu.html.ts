// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContextualTasksInnerComposeboxElement} from './contextual_tasks_inner_composebox.js';

export function getHtml(this: ContextualTasksInnerComposeboxElement) {
  // Contextual Tasks always keeps `showMenuOnClick` (menu, never the bare
  // entrypoint button) and never uses the Compact layout, so the shared
  // template's entrypoint-button and voice-search branches are omitted.
  // clang-format off
  return html`
<div class="context-menu-container" id="contextMenuContainer"
    part="context-menu-and-tools"
    @mousedown="${this.onContextMenuContainerMousedown}"
    @click="${this.onContextMenuContainerClick}">
  <cr-composebox-contextual-entrypoint-and-menu
      id="contextEntrypoint"
      part="composebox-entrypoint"
      exportparts="context-menu-entrypoint-icon, entrypoint-button"
      class="upload-button no-overlap"
      @add-tab-context="${this.onAddTabContext}"
      @delete-tab-context="${this.onDeleteTabContext}"
      @tool-click="${this.onToolClick}"
      @model-click="${this.onModelClick}"
      @get-tab-preview="${this.onGetTabPreview}"
      @wait-for-tab-load="${this.onWaitForTabLoad}"
      @context-menu-closed="${this.onContextMenuClosed}"
      @context-menu-opened="${this.onContextMenuOpened}"
      @open-image-upload="${this.onOpenImageUpload}"
      @open-file-upload="${this.onOpenFileUpload}"
      @open-drive-upload="${this.onOpenDriveUpload}"
      @smart-tab-sharing-active-changed="${
          this.onSmartTabSharingActiveChanged}"
      @share-tabs-flyout-open-changed="${this.onShareTabsFlyoutOpenChanged}"
      @request-tab-suggestions-load="${this.onRequestTabSuggestionsLoad}"
      .inputState="${this.inputState}"
      .usePecApi="${this.usePecApi}"
      .smartTabSharingActive="${this.smartTabSharingActive}"
      .smartTabSharingVisible="${this.smartTabSharingVisible}"
      .shareTabsFlyoutOpen="${this.shareTabsFlyoutOpen}"
      .contextManagementInComposeboxEnabled="${this.contextManagementInComposeboxEnabled}"
      .searchboxLayoutMode="${this.searchboxLayoutMode}"
      .tabSuggestions="${this.tabSuggestions}"
      .tabSuggestionsState="${this.tabSuggestionsState}"
      .recentTabId="${this.recentTabId}"
      .hasImageFiles="${this.hasImageFiles()}"
      .disabledTabIds="${this.addedTabsIds}"
      .aimThreadRestoredTabs="${this.aimThreadRestoredTabs}"
      .fileNum="${this.files.size}"
      .nonTabFileNum="${this.getNonTabFileNum()}"
      .sharedTabs="${this.getSharedTabs()}"
      .isSidePanel="${this.isSidePanel}"
      ?upload-button-disabled="${this.uploadButtonDisabled}"
      ?show-context-menu-description="${this.showContextMenuDescription}"
      .glifAnimationState="${this.glifAnimationState}"
      .energyEffectAnimationEnabled="${this.energyEffectAnimationEnabled}"
      .disableFallbackGlifAnimation="${this.disableFallbackGlifAnimation}">
  </cr-composebox-contextual-entrypoint-and-menu>
  ${this.searchboxLayoutMode !== 'Compact' ? html`
    ${this.inToolMode ? html`
      <cr-composebox-tool-chip
        exportparts="tool-chip-label"
        .inputState="${this.inputState}"
        .isCanvasQuerySubmitted="${this.isCanvasQuerySubmitted}"
        @tool-click="${this.onToolClick}">
      </cr-composebox-tool-chip>
    ` : ''}
  ` : ''}
</div>
  `;
  // clang-format on
}
