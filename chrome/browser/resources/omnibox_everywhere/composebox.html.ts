// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {hasAllowedInputs} from '//resources/cr_components/composebox/common.js';
import {ToolMode} from '//resources/cr_components/composebox/composebox_query.mojom-webui.js';
import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OmniboxEverywhereComposeboxElement} from './composebox.js';

export function getHtml(this: OmniboxEverywhereComposeboxElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    <div id="shadow-container"></div>
    ${!this.disableComposeboxAnimation ? html`
      <search-animated-glow id="animatedSearchElement"
          animation-state="${this.animationState}"
          .coloredTicTacVoiceAnimationEnabled="${false}"
          .isListening="${this.isListening}"
          .entrypointName="${this.entrypointName}"
          .requiresVoice="${this.shouldShowVoiceSearchAnimation()}"
          .transcript="${this.transcript}"
          .receivedSpeech="${this.receivedSpeech}"
          .energyEffectAnimationEnabled="${false}"
          .isZeroState="${false}"
          exportparts="composebox-background">
      </search-animated-glow>
    ` : ''}
    <div id="composebox" part="composebox" ?inert="${!!this.errorMessage}"
      @keydown="${this.onKeydown}">
      <div id="inputContainer" part="input-container">
        <!-- Note: Copied from omnibox_composebox.html.ts. Cancel button title
             and cancel click handler may be needed if added to mixin in the
             future. -->
        <cr-composebox-input id="composeboxInput"
            exportparts="text-container, icon-container, mirror, input,
                         smart-compose, cancel, action-icon, cancel-icon"
            .disableCaretColorAnimation="${this.disableCaretColorAnimation}"
            .showDropdown="${this.showDropdown}"
            .inputPlaceholder="${this.inputPlaceholder}"
            .input="${this.input}"
            .smartComposeInlineHint="${this.smartComposeInlineHint}"
            .submitEnabled="${this.submitEnabled}"
            .entrypointName="${this.entrypointName}"
            @input-input="${this.onInputInput}"
            @input-focusin="${this.onInputFocusin}">
        </cr-composebox-input>
        <div id="context" part="context-entrypoint">
          <!-- Note: Copied from omnibox_composebox.html.ts. May need to re-add
               shouldDisableFileInputs_ when added to mixin. -->
          <cr-composebox-file-inputs id="fileInputs"
              @file-change="${this.onFileChange}">
            <!-- Note: May need to add file carousel in a future iteration. -->
                          ${this.shouldShowDivider() ? html`
            <div class="carousel-divider" part="carousel-divider"></div>
            ` : ''}
            <cr-composebox-dropdown
                id="matches"
                part="dropdown"
                exportparts="match-text-container"
                role="listbox"
                .result="${this.result}"
                .selectedMatchIndex="${this.selectedMatchIndex}"
                .maxSuggestions="${this.maxSuggestions}"
                .toolMode="${this.inputState?.activeTool ||
                             ToolMode.kUnspecified}"
                @selected-match-index-changed="${
                    this.onSelectedMatchIndexChanged}"
                @match-focusin="${this.onMatchFocusin}"
                @match-click="${this.onMatchClick}"
                ?hidden="${!this.showDropdown || !this.dropdownNeeded}"
                .lastQueriedInput="${this.lastQueriedInput}">
            </cr-composebox-dropdown>
            ${this.contextMenuEnabled ? html`
              <div class="context-menu-container" id="contextMenuContainer"
                  part="context-menu-and-tools"
                  @mousedown="${this.onContextMenuContainerMousedown}"
                  @click="${this.onContextMenuContainerClick}">
                ${hasAllowedInputs(this.inputState, this.usePecApi) ? html`
                  <cr-composebox-contextual-entrypoint-and-menu
                      id="contextEntrypoint"
                      part="composebox-entrypoint"
                      exportparts="context-menu-entrypoint-icon,
                                   entrypoint-button"
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
                      .inputState="${this.inputState}"
                      .usePecApi="${this.usePecApi}"
                      .smartTabSharingActive="${this.smartTabSharingActive}"
                      .contextManagementInComposeboxEnabled="${this.contextManagementInComposeboxEnabled}"
                      .searchboxLayoutMode="${this.searchboxLayoutMode}"
                      .tabSuggestions="${this.tabSuggestions}"
                      .recentTabId="${this.recentTabId}"
                      .hasImageFiles="${this.hasImageFiles()}"
                      .disabledTabIds="${this.addedTabsIds}"
                      .aimThreadRestoredTabs="${this.aimThreadRestoredTabs}"
                      .fileNum="${this.files.size}"
                      .sharedTabs="${this.getSharedTabs()}"
                      ?upload-button-disabled="${this.uploadButtonDisabled}"
                      ?show-context-menu-description="${
                          this.showContextMenuDescription}">
                  </cr-composebox-contextual-entrypoint-and-menu>
                ` : ''}
                ${this.searchboxLayoutMode !== 'Compact' &&
                  this.inToolMode ? html`
                  <cr-composebox-tool-chip
                    exportparts="tool-chip-label"
                    .inputState="${this.inputState}"
                    .isCanvasQuerySubmitted="${this.isCanvasQuerySubmitted}"
                    @tool-click="${this.onToolClick}">
                  </cr-composebox-tool-chip>
                ` : ''}
              </div>
            ` : ''}
            ${this.shouldShowSubmitButton() ? html`
              <cr-composebox-submit
                exportparts="action-icon, submit, submit-icon, submit-overlay"
                ?disabled="${!this.canSubmitFilesAndInput}"
                .iconType="${
                  // eslint-disable-next-line @typescript-eslint/no-explicit-any
                  'forward' as any
                }"
                .submitButtonTitle="${this.i18n('composeboxSubmitButtonTitle')}"
                @submit-click="${this.onSubmitClick}"
                @submit-focusin="${this.onSubmitFocusin}">
              </cr-composebox-submit>
            ` : ''}
          </cr-composebox-file-inputs>
        </div>
      </div>
    </div>
<!--_html_template_end_-->`;
  // clang-format on
}
