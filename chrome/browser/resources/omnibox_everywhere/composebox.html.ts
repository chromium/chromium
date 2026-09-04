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
          .energyEffectAnimationEnabled="${this.energyEffectAnimationEnabled}"
          .isZeroState="${false}"
          exportparts="composebox-background">
      </search-animated-glow>
    ` : ''}
    <div id="composebox" part="composebox" ?inert="${!!this.errorMessage}"
      @keydown="${this.onKeydown}"
      @dragenter="${this.dragAndDropHandler.handleDragEnter}"
      @dragover="${this.dragAndDropHandler.handleDragOver}"
      @dragleave="${this.dragAndDropHandler.handleDragLeave}"
      @drop="${this.dragAndDropHandler.handleDrop}"
      @paste="${this.onPaste}">
      <div id="inputContainer" part="input-container">
        <cr-composebox-input id="composeboxInput"
            exportparts="text-container, icon-container, mirror, input,
                         smart-compose, cancel, action-icon, cancel-icon"
            .composeboxSkillsEnabled="${this.composeboxSkillsEnabled}"
            .disableCaretColorAnimation="${this.disableCaretColorAnimation}"
            .showDropdown="${this.showDropdown}"
            .inputPlaceholder="${this.inputPlaceholder}"
            .input="${this.input}"
            .smartComposeEnabled="${this.smartComposeEnabled}"
            .smartComposeInlineHint="${this.smartComposeInlineHint}"
            .submitEnabled="${this.submitEnabled}"
            .entrypointName="${this.entrypointName}"
            .cancelButtonTitle="${this.computeCancelButtonTitle()}"
            @input-input="${this.onInputInput}"
            @input-focusin="${this.onInputFocusin}"
            @cancel-click="${this.onCancelClick}"
            @clear-smart-compose="${this.onClearSmartCompose}">
        </cr-composebox-input>
        <div id="context" part="context-entrypoint">
          <!-- Note: Copied from omnibox_composebox.html.ts. May need to re-add
               shouldDisableFileInputs_ when added to mixin. -->
          <cr-composebox-file-inputs id="fileInputs"
              @file-change="${this.onFileChange}">
            ${this.showFileCarousel ? html`
              <cr-composebox-file-carousel
                  id="carousel"
                  part="cr-composebox-file-carousel"
                  exportparts="thumbnail, thumbnail-title"
                  .files="${this.getFilteredCarouselFiles()}"
                  @delete-file="${this.onDeleteFile}">
              </cr-composebox-file-carousel>
            ` : ''}
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
            <div id="bottomControls">
              ${this.contextMenuEnabled ? html`
                <div class="context-menu-container" id="contextMenuContainer"
                    part="context-menu-and-tools"
                    @mousedown="${this.onContextMenuContainerMousedown}"
                    @click="${this.onContextMenuContainerClick}">
                  ${hasAllowedInputs(this.inputState, this.usePecApi) ? html`
                    <cr-composebox-contextual-entrypoint-button
                        id="contextEntrypoint"
                        part="composebox-entrypoint"
                        exportparts="context-menu-entrypoint-icon,
                                     entrypoint-button"
                        class="upload-button no-overlap"
                        .inputState="${this.inputState}"
                        .sharedTabs="${this.getSharedTabs()}"
                        .restoredTabs="${this.aimThreadRestoredTabs}"
                        .smartTabSharingActive="${this.smartTabSharingActive}"
                        .energyEffectAnimationEnabled="${
                            this.energyEffectAnimationEnabled}"
                        ?upload-button-disabled="${this.uploadButtonDisabled}"
                        ?show-context-menu-description="${
                            this.showContextMenuDescription}"
                        @context-menu-entrypoint-click="${
                            this.onContextMenuEntrypointClick_}">
                    </cr-composebox-contextual-entrypoint-button>
                  ` : ''}
                  ${this.inToolMode ? html`
                    <cr-composebox-tool-chip
                      exportparts="tool-chip-label"
                      .inputState="${this.inputState}"
                      .isCanvasQuerySubmitted="${this.isCanvasQuerySubmitted}"
                      @tool-click="${this.onToolClick}">
                    </cr-composebox-tool-chip>
                  ` : ''}
                </div>
              ` : ''}
              <div id="actionButtons">
                ${this.shouldShowVoiceSearch() ? html`
                <div class="searchbox-icon-button-container voice">
                  <button id="voiceSearchButton" class="searchbox-icon-button"
                      @click="${this.onVoiceSearchButtonClick}"
                      title="${this.i18n('voiceSearchButtonLabel')}">
                  </button>
                </div>
                ` : ''}
                <div class="searchbox-icon-button-container lens ${
                    this.isScreenshotMenuOpen ? 'menu-open' : ''}">
                  <button id="lensSearchButton" class="searchbox-icon-button"
                      @click="${this.onLensSearchClick_}"
                      title="${this.i18n('lensSearchButtonLabel')}">
                  </button>
                </div>
                ${this.shouldShowSubmitButton() ? html`
                  <cr-composebox-submit
                    exportparts="action-icon, submit, submit-icon,
                                 submit-overlay"
                    ?disabled="${!this.canSubmitFilesAndInput}"
                    .iconType="${this.submitButtonIconType}"
                    .submitButtonTitle="${
                        this.i18n('composeboxSubmitButtonTitle')}"
                    @submit-click="${this.onSubmitClick}"
                    @submit-focusin="${this.onSubmitFocusin}">
                  </cr-composebox-submit>
                ` : ''}
              </div>
            </div>
          </cr-composebox-file-inputs>
        </div>
    </div>
<!--_html_template_end_-->`;
  // clang-format on
}
