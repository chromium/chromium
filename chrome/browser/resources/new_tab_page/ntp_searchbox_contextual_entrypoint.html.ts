// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {NtpSearchboxElement} from './ntp_searchbox.js';

export function getHtml(this: NtpSearchboxElement) {
  // clang-format off
  return html`
<cr-composebox-file-inputs @file-change="${this.onFileChange_}">
  <div class="context-menu-container" id="contextMenuContainer">
    <cr-composebox-contextual-entrypoint-and-menu id="context"
        exportparts="context-menu-entrypoint-icon"
        class="upload-button"
        disable-auto-reposition
        @add-tab-context="${this.onAddTabContext_}"
        @tool-click="${this.onToolClick_}"
        @deep-search-click="${this.onDeepSearchClick_}"
        @create-image-click="${this.onCreateImageClick_}"
        @model-click="${this.onModelClick_}"
        @get-tab-preview="${this.onGetTabPreview_}"
        @context-menu-entrypoint-click="${this.onContextMenuEntrypointClick_}"
        @context-menu-entrypoint-hover="${this.onContextMenuEntrypointHover_}"
        @context-menu-closed="${this.onContextMenuClosed_}"
        @context-menu-opened="${this.onContextMenuOpened_}"
        @open-drive-upload="${this.onOpenDriveUpload_}"
        @request-tab-suggestions-load="${this.onRequestTabSuggestionsLoad}"
        .inputState="${this.inputState_}"
        .searchboxLayoutMode="${this.searchboxLayoutMode}"
        .tabSuggestions="${this.tabSuggestions_}"
        .tabSuggestionsLoading="${this.tabSuggestionsLoading_}"
        .tabSuggestionsHasLoaded="${this.tabSuggestionsHasLoaded_}"
        .recentTabId="${this.recentTabId_}"
        ?show-context-menu-description="${!this.useCompactLayout_()}"
        .glifAnimationState="${this.contextMenuGlifAnimationState}"
        .energyEffectAnimationEnabled="${this.energyEffectAnimationEnabled}">
    </cr-composebox-contextual-entrypoint-and-menu>
  </div>
</cr-composebox-file-inputs>
`;
  // clang-format on
}
