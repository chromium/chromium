// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {NtpComposeboxElement} from './ntp_composebox.js';

export function getHtml(this: NtpComposeboxElement) {
  // clang-format off
  return html`
<div class="context-menu-container" id="contextMenuContainer"
    part="context-menu-and-tools"
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
      @context-menu-closed="${this.onContextMenuClosed}"
      @context-menu-opened="${this.onContextMenuOpened}"
      @open-image-upload="${this.onOpenImageUpload}"
      @open-file-upload="${this.onOpenFileUpload}"
      @open-drive-upload="${this.onOpenDriveUpload}"
      @smart-tab-sharing-active-changed="${this.onSmartTabSharingActiveChanged}"
      @share-tabs-flyout-open-changed="${this.onShareTabsFlyoutOpenChanged}"
      @request-tab-suggestions-load="${this.onRequestTabSuggestionsLoad}"
      .shareTabsFlyoutOpen="${this.shareTabsFlyoutOpen}"
      .inputState="${this.inputState}"
      .usePecApi="${this.usePecApi}"
      .smartTabSharingActive="${this.smartTabSharingActive}"
      .searchboxLayoutMode="${this.searchboxLayoutMode}"
      .tabSuggestions="${this.tabSuggestions}"
      .hasImageFiles="${this.hasImageFiles()}"
      .disabledTabIds="${this.addedTabsIds}"
      .aimThreadRestoredTabs="${this.aimThreadRestoredTabs}"
      .fileNum="${this.files.size}"
      .sharedTabs="${this.getSharedTabs()}"
      .tabSuggestionsState="${this.tabSuggestionsState}"
      ?upload-button-disabled="${this.uploadButtonDisabled}"
      ?show-context-menu-description="${this.showContextMenuDescription}">
  </cr-composebox-contextual-entrypoint-and-menu>
  ${this.shouldShowVoiceSearch() ? html`
    <cr-icon-button id="voiceSearchButton" class="voice-icon"
        part="voice-icon" iron-icon="cr:mic"
        @click="${this.onVoiceSearchButtonClick}"
        title="${this.i18n('voiceSearchButtonLabel')}">
    </cr-icon-button>
  ` : ''}
</div>
  `;
  // clang-format on
}
