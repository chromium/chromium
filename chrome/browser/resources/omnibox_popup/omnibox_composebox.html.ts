// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {hasAllowedInputs} from '//resources/cr_components/composebox/common.js';
import {ToolMode} from '//resources/cr_components/composebox/composebox_query.mojom-webui.js';
import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OmniboxComposeboxElement} from './omnibox_composebox.js';

export function getHtml(this: OmniboxComposeboxElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    <search-animated-glow id="animatedSearchElement"
        animation-state="${this.animationState}"
        .coloredTicTacVoiceAnimationEnabled=
            "${this.voiceSearchCoherenceEnabled}"
        .isListening="${this.isListening}"
        .entrypointName="${this.entrypointName}"
        .requiresVoice="${this.shouldShowVoiceSearchAnimation()}"
        .transcript="${this.transcript}"
        .receivedSpeech="${this.receivedSpeech}"
        .energyEffectAnimationEnabled="${false}"
        .isZeroState="${false}"
        exportparts="composebox-background">
    </search-animated-glow>
    <ntp-error-scrim id="errorScrim" part="error-scrim"
        ?compact-mode="${this.searchboxLayoutMode === 'Compact' &&
                         this.files.size === 0}"
        .errorMessage="${this.errorMessage}"
        @dismiss-error-scrim="${this.onDismissErrorScrim}">
    </ntp-error-scrim>
    <div id="composebox" part="composebox" ?inert="${!!this.errorMessage}"
      @keydown="${this.onKeydown}">
      <div id="inputContainer" part="input-container">
        <cr-composebox-input id="composeboxInput"
            exportparts="text-container, icon-container, mirror, input, smart-compose, cancel, action-icon, cancel-icon"
            .disableCaretColorAnimation="${this.disableCaretColorAnimation}"
            .showDropdown="${this.showDropdown}"
            .inputPlaceholder="${this.inputPlaceholder}"
            .input="${this.input}"
            .smartComposeInlineHint="${this.smartComposeInlineHint}"
            .submitEnabled="${this.submitEnabled}"
            .entrypointName="${this.entrypointName}"
            .cancelButtonTitle="${this.computeCancelButtonTitle()}"
            @input-input="${this.onInputInput}"
            @input-focusin="${this.onInputFocusin}"
            @cancel-click="${this.onCancelClick}">
        </cr-composebox-input>
        <div id="context" part="context-entrypoint">
          <div id="carouselContainer" part="carousel-container">
            <div class="carousel-container-inner">
              ${this.showFileCarousel ? html`
                <cr-composebox-file-carousel
                  part="cr-composebox-file-carousel"
                  exportparts="thumbnail, thumbnail-title"
                  id="carousel"
                  .files="${this.getFilteredCarouselFiles()}"
                  ?enable-scrolling="${this.enableCarouselScrolling}"
                  @delete-file="${this.onDeleteFile}">
                </cr-composebox-file-carousel> ` : ''}
            </div>
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
          ${this.contextMenuEnabled ? html`
            <div class="context-menu-container" id="contextMenuContainer"
                part="context-menu-and-tools"
                @mousedown="${this.onContextMenuContainerMousedown}"
                @click="${this.onContextMenuContainerClick}">
              ${hasAllowedInputs(this.inputState, this.usePecApi) ? html`
                <cr-composebox-contextual-entrypoint-button
                    id="contextEntrypoint"
                    part="composebox-entrypoint"
                    exportparts="context-menu-entrypoint-icon, entrypoint-button"
                    class="upload-button no-overlap"
                    .inputState="${this.inputState}"
                    .applyContextButtonBackground="${this.applyContextButtonBackground}"
                    .isOblongShape="${this.isOblongShape}"
                    ?upload-button-disabled="${this.uploadButtonDisabled}"
                    ?show-context-menu-description="${this.showContextMenuDescription}">
                </cr-composebox-contextual-entrypoint-button>
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
          ${this.shouldShowVoiceSearchAtBottom() ? html`
            <cr-icon-button id="voiceSearchButton" class="voice-icon" part="voice-icon"
                iron-icon="cr:mic" @click="${this.onVoiceSearchButtonClick}"
                title="${this.i18n('voiceSearchButtonLabel')}">
            </cr-icon-button>
          ` : ''}
          ${this.shouldShowSubmitButton() &&
                this.searchboxLayoutMode === 'TallBottomContext' ? html`
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
    ${this.shouldShowSuggestionActivityLink() ? html`
      <div id="suggestionActivity">
        <localized-link
          .localizedString="${this.i18nAdvanced('suggestionActivityLink')}"
          @link-clicked="${this.onLinkClicked}">
        </localized-link>
      </div>
    `: ''}
<!--_html_template_end_-->`;
  // clang-format on
}
