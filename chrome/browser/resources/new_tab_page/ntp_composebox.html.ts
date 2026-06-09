// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ToolMode} from '//resources/cr_components/composebox/composebox_query.mojom-webui.js';
import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {NtpComposeboxElement} from './ntp_composebox.js';
import {getHtml as getContextMenuHtml} from './ntp_composebox_context_menu.html.js';


export function getHtml(this: NtpComposeboxElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    <search-animated-glow id="animatedSearchElement"
        animation-state="${this.animationState}"
        .coloredTicTacVoiceAnimationEnabled="${this.voiceSearchCoherenceEnabled}"
        .requiresVoice="${this.shouldShowVoiceSearchAnimation()}"
        .transcript="${this.transcript}"
        .receivedSpeech="${this.receivedSpeech}"
        .isListening="${this.isListening}"
        exportparts="composebox-background">
    </search-animated-glow>
    <ntp-error-scrim id="errorScrim" part="error-scrim"
        ?compact-mode="${this.files.size === 0}"
        .errorMessage="${this.errorMessage}"
        @dismiss-error-scrim="${this.onDismissErrorScrim}">
    </ntp-error-scrim>
    <div id="composebox" part="composebox" ?inert="${!!this.errorMessage}"
        @keydown="${this.onKeydown}"
        @dragenter="${this.dragAndDropHandler_.handleDragEnter}"
        @dragover="${this.dragAndDropHandler_.handleDragOver}"
        @dragleave="${this.dragAndDropHandler_.handleDragLeave}"
        @drop="${this.dragAndDropHandler_.handleDrop}"
        @paste="${this.onPaste}">
      <div id="inputContainer" part="input-container">
        <cr-composebox-input id="composeboxInput"
            class="${this.hasTabs() ? 'has-tabs' : ''}"
            exportparts="text-container, icon-container, mirror, input, smart-compose, cancel, action-icon, cancel-icon"
            .disableCaretColorAnimation="${this.disableCaretColorAnimation}"
            .showDropdown="${this.showDropdown}"
            .inputPlaceholder="${this.inputPlaceholder}"
            .input="${this.input}"
            .smartComposeEnabled="${this.smartComposeEnabled}"
            .smartComposeInlineHint="${this.smartComposeInlineHint}"
            .submitEnabled="${this.submitEnabled}"
            .cancelButtonTitle="${this.computeCancelButtonTitle()}"
            @input-input="${this.onInputInput}"
            @input-focusin="${this.onInputFocusin}"
            @cancel-click="${this.onCancelClick}">
        </cr-composebox-input>
        <div id="context" part="context-entrypoint">
          <cr-composebox-file-inputs id="fileInputs"
              @file-change="${this.onFileChange}"
              .disableFileInputs="${this.shouldDisableFileInputs()}">
            ${this.hasTabs() ? '' : getContextMenuHtml.bind(this)()}
            <div id="carouselContainer" part="carousel-container">
              <div class="carousel-container-inner">
                ${this.showFileCarousel ? html`
                  <cr-composebox-file-carousel
                    part="cr-composebox-file-carousel"
                    exportparts="thumbnail, thumbnail-title"
                    id="carousel"
                    .files="${this.getFilteredCarouselFiles()}"
                    @delete-file="${this.onDeleteFile}">
                  </cr-composebox-file-carousel> ` : ''}
                  ${this.hasTabs() && this.contextMenuEnabled ? html`
                    ${getContextMenuHtml.bind(this)()}
                  ` : ''}
                  ${this.inToolMode ? html`
                  <div class="context-menu-container" id="toolChipsContainer"
                      part="tool-chips-container">
                      <cr-composebox-tool-chip
                        exportparts="tool-chip-label"
                        .inputState="${this.inputState}"
                        .isCanvasQuerySubmitted="${this.isCanvasQuerySubmitted}"
                        @tool-click="${this.onToolClick}"
                        part="tool-chip">
                      </cr-composebox-tool-chip>
                  </div>
                  ` : ''}
              </div>
              ${this.shouldShowSubmitButton() ? html`
              <cr-composebox-submit
                exportparts="action-icon, submit, submit-icon, submit-overlay"
                ?disabled="${!this.canSubmitFilesAndInput}"
                .iconType="${this.submitButtonIconType}"
                .submitButtonTitle="${this.i18n('composeboxSubmitButtonTitle')}"
                @submit-click="${this.onSubmitClick}"
                @submit-focusin="${this.onSubmitFocusin}">
              </cr-composebox-submit>
              ` : ''}
            </div>
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
                .toolMode="${this.inputState?.activeTool || ToolMode.kUnspecified}"
                @selected-match-index-changed="${this.onSelectedMatchIndexChanged}"
                @match-focusin="${this.onMatchFocusin}"
                @match-click="${this.onMatchClick}"
                ?hidden="${!this.showDropdown || !this.dropdownNeeded}"
                .lastQueriedInput="${this.lastQueriedInput}">
            </cr-composebox-dropdown>
          </cr-composebox-file-inputs>
        </div>
      </div>
    </div>
  ${this.shouldShowVoiceSearch() ? html`
    <cr-composebox-voice-search id="voiceSearch"
        @voice-permission-changed="${this.onVoicePermissionChanged}"
        @voice-search-cancel="${this.onVoiceSearchCancel}"
        @voice-search-final-result="${this.onVoiceSearchFinalResult}"
        @voice-search-error="${this.onVoiceSearchError}"
        @transcript-update="${this.onTranscriptUpdate}"
        @speech-received="${this.onSpeechReceived}"
        @recording-stopped="${this.onRecordingStopped}"
        .submitStopButtonsEnabled="${this.voiceSearchCoherenceEnabled}"
        .liveTranscriptEnabled="${!this.voiceSearchCoherenceEnabled}"
        .submitButtonIconType="${this.submitButtonIconType}"
        .dynamicTimeoutEnabled="${false}"
        .pageCallbackRouter="${this.getSearchboxCallbackRouter()}"
        exportparts="voice-close-button, voice-details-link, voice-stop-button, voice-submit-button">
    </cr-composebox-voice-search>
  ` : ''}
  <!--_html_template_end_-->`;
  // clang-format on
}
