// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import {ToolMode} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';

import type {ContextualTasksInnerComposeboxElement} from './contextual_tasks_inner_composebox.js';
import {getHtml as getContextMenuHtml} from './contextual_tasks_inner_composebox_context_menu.html.js';

export function getHtml(this: ContextualTasksInnerComposeboxElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    <search-animated-glow id="animatedSearchElement"
        animation-state="${this.animationState}"
        .coloredTicTacVoiceAnimationEnabled="${this.voiceSearchCoherenceEnabled}"
        .isListening="${this.isListening}"
        .requiresVoice="${this.shouldShowVoiceSearchAnimation()}"
        .transcript="${this.transcript}"
        .receivedSpeech="${this.receivedSpeech}"
        .energyEffectAnimationEnabled="${this.energyEffectAnimationEnabled}"
        .entrypointName="${this.entrypointName}"
        .isZeroState="${this.isZeroState}"
        .darkThemeColorsEnabled="${true}"
        .showingOnlyCarouselOnTopOfInput="${this.showFileCarousel &&
            !this.inToolMode && this.carouselOnTop_ &&
            this.voiceSearchCoherenceEnabled}"
        exportparts="composebox-background">
    ${this.showFileCarousel && this.shouldShowVoiceSearchAnimation() &&
        this.voiceSearchCoherenceEnabled ? html`
      <div id="voiceCarouselContainer"
          slot="carousel"
          part ="carousel-container">
        <div id="voiceCarouselContainerInner"
            class="carousel-container-inner">
          <cr-composebox-file-carousel
            id="voiceSearchCarousel"
            class="${this.carouselOnTop_ ? 'top' : ''}"
            .files="${this.getFilteredCarouselFiles()}"
            enable-scrolling
            @delete-file="${this.onDeleteFile}">
          </cr-composebox-file-carousel>
        </div>
      </div>
    ` : ''}
    ${this.shouldShowVoiceSearchAnimation() &&
        this.voiceSearchCoherenceEnabled && this.inToolMode ? html`
      <div class="context-menu-container voice-context-menu-container"
          id="voiceToolChipsContainer"
          slot="tool-chip"
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
    </search-animated-glow>
    ${this.errorMessage ?
        html`<ntp-error-scrim id="errorScrim" part="error-scrim"
            .errorMessage="${this.errorMessage}"
            @dismiss-error-scrim="${this.onDismissErrorScrim}">
        </ntp-error-scrim>`
    : ''}
    <div id="composebox" part="composebox" ?inert="${!!this.errorMessage}"
        @keydown="${this.onKeydown}"
        @focusin="${this.onComposeboxFocusin_}"
        @focusout="${this.onComposeboxFocusout_}"
        @dragenter="${this.dragAndDropHandler.handleDragEnter}"
        @dragover="${this.dragAndDropHandler.handleDragOver}"
        @dragleave="${this.dragAndDropHandler.handleDragLeave}"
        @drop="${this.dragAndDropHandler.handleDrop}"
        @paste="${this.onPaste}">
      <div id="inputContainer" part="input-container">
        <cr-composebox-input id="composeboxInput"
            exportparts="text-container, icon-container, mirror, input, smart-compose, cancel, action-icon, cancel-icon"
            .composeboxSkillsEnabled="${this.composeboxSkillsEnabled}"
            .disableCaretColorAnimation="${this.disableCaretColorAnimation}"
            .entrypointName="${this.entrypointName}"
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
        <cr-composebox-file-inputs id="fileInputs"
            @file-change="${this.onFileChange}"
            .disableFileInputs="${this.shouldDisableFileInputs()}">
          <div id="carouselContainer" part="carousel-container">
            <div class="carousel-container-inner">
              ${this.showFileCarousel ? html`
                <cr-composebox-file-carousel
                  part="cr-composebox-file-carousel"
                  exportparts="thumbnail, thumbnail-title"
                  id="carousel"
                  class="${this.carouselOnTop_ ? 'top' : ''}"
                  .files="${this.getFilteredCarouselFiles()}"
                  ?enable-scrolling="${this.enableCarouselScrolling}"
                  @delete-file="${this.onDeleteFile}">
                </cr-composebox-file-carousel> ` : ''}
            </div>
          </div>
          ${this.shouldShowDivider() ? html`
          <div class="carousel-divider" part="carousel-divider"></div>
          ` : ''}
          <cr-composebox-dropdown id="matches" part="dropdown"
              exportparts="match-text-container"
              role="listbox"
              .result="${this.result}"
              .selectedMatchIndex="${this.selectedMatchIndex}"
              .maxSuggestions="${this.maxSuggestions}"
              .toolMode="${this.inputState?.activeTool || ToolMode.kUnspecified}"
              .lastQueriedInput="${this.lastQueriedInput}"
              ?hidden="${!this.showDropdown || !this.dropdownNeeded}"
              @selected-match-index-changed="${this.onSelectedMatchIndexChanged}"
              @match-focusin="${this.onMatchFocusin}"
              @match-click="${this.onMatchClick}">
          </cr-composebox-dropdown>
          ${this.contextMenuEnabled ? getContextMenuHtml.bind(this)() : ''}
          ${this.shouldShowVoiceSearchAtBottom() ? html`
            <cr-icon-button id="voiceSearchButton" class="voice-icon"
                part="voice-icon"
                iron-icon="cr:mic" @click="${this.onVoiceSearchButtonClick}"
                title="${this.i18n('voiceSearchButtonLabel')}">
            </cr-icon-button>
          ` : ''}
        </cr-composebox-file-inputs>
      </div>
      ${this.showLensButton ? html`<cr-icon-button
          class="action-icon"
          id="lensIcon"
          part="action-icon lens-icon"
          title="${this.i18n('lensSearchButtonLabel')}"
          @click="${this.onLensClick_}"
          ?disabled="${this.lensButtonDisabled}"
          @mousedown="${this.onLensIconMousedown_}">
      </cr-icon-button>` : ''}
      <cr-composebox-submit
          exportparts="action-icon, submit, submit-icon, submit-overlay"
          ?disabled="${!this.canSubmitFilesAndInput}"
          .iconType="${this.submitButtonIconType}"
          .submitButtonTitle="${this.i18n('composeboxSubmitButtonTitle')}"
          @submit-click="${this.onSubmitClick}"
          @submit-focusin="${this.onSubmitFocusin}">
      </cr-composebox-submit>
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
    ` : '' }
  <!--_html_template_end_-->`;
  // clang-format on
}
