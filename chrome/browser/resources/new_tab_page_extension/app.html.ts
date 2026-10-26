// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SubmitButtonIconType} from 'chrome://resources/cr_components/composebox/common.js';
import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {NtpExtensionAppElement} from './app.js';

// Mirrors the logo / searchbox / composebox / action chips / Lens / voice
// search part of
// chrome/browser/resources/new_tab_page/app.html.
export function getHtml(this: NtpExtensionAppElement) {
  return html`
<div id="content">
  <ntp-logo id="logo" ?single-colored="${this.isLogoSingleColored_()}"
      .theme="${this.theme_}">
  </ntp-logo>
  ${this.ntpRealboxNextEnabled_ ? html`
    <div id="scrim" @click="${this.onScrimClick_}" ?hidden="${!this.showScrim_}">
    </div>
  ` : ''}
  <div id="searchboxContainer"
      @focusin="${this.onSearchboxContainerFocusin_}"
      @focusout="${this.onSearchboxContainerFocusout_}">
    <ntp-searchbox id="searchbox" ?is-dark="${this.isThemeDark_()}"
        placeholder-text="${this.i18n('searchBoxPlaceholder')}"
        ?color-source-is-baseline="${this.colorSourceIsBaseline_}"
        @open-composebox="${this.onOpenComposebox_}"
        @open-lens-search="${this.onOpenLensSearch_}"
        @open-voice-search="${this.onOpenVoiceSearch_}"
        .hasVoiceSearchError="${this.hasVoiceSearchError_}"
        .inVoiceSearchMode="${this.showVoiceSearch_}"
        ?shown="${this.realboxShown_}"
        @context-menu-entrypoint-click="${this.onContextMenuEntrypointClick_}"
        ?can-show-secondary-side="${this.realboxCanShowSecondarySide}"
        ?compose-button-enabled="${this.composeButtonEnabled_}"
        ?composebox-enabled="${this.composeboxEnabled_}"
        searchbox-layout-mode="${this.ntpRealboxNextEnabled_ ? 'Compact' : ''}"
        ?ntp-realbox-next-enabled="${this.ntpRealboxNextEnabled_}"
        ?cycling-placeholders="${this.searchboxCyclingPlaceholders_}"
        context-menu-glif-animation-state="${this.contextMenuGlifAnimationState_}"
        .energyEffectAnimationEnabled="${this.energyEffectAnimationEnabled_}"
        .contextManagementInComposeboxEnabled="${this.contextManagementInComposeboxEnabled_}"
        ?multi-line-enabled="${this.multiLineEnabled_}"
        ?inert="${!this.realboxShown_}">
    </ntp-searchbox>
    <div id="dialogAnchor"></div>
    ${this.showComposebox_ ? html`
      <div id="composeboxDialog">
        <ntp-composebox id="composebox"
            searchbox-layout-mode="Compact"
            searchbox-next-enabled
            ?disable-caret-color-animation="${!this.caretAnimationsEnabled_}"
            .state="${this.composeboxState_}"
            .usePecApi="${this.usePecApi_}"
            .showVoiceSearch="${true}"
            .smartTabSharingVisible="${this.smartTabSharingVisible_}"
            .contextManagementInComposeboxEnabled="${this.contextManagementInComposeboxEnabled_}"
            @context-menu-opened="${this.onComposeboxContextMenuOpened_}"
            @close-composebox="${this.onCloseComposebox_}"
            .submitButtonIconType="${SubmitButtonIconType.FORWARD}"
            .clearAllInputsWhenSubmittingQuery="${true}"
            .closeOnEscape="${false}"
            .energyEffectEnabled="${this.energyEffectEnabled_}"
            .energyEffectAnimationEnabled="${this.energyEffectAnimationEnabled_}">
        </ntp-composebox>
      </div>
    ` : ''}
    ${this.showLensUploadDialog_ ? html`
      <ntp-lens-upload-dialog id="lensUploadDialog"
          @close-lens-search="${this.onCloseLensSearch_}">
      </ntp-lens-upload-dialog>
    ` : ''}
  </div>
  ${this.showActionChips_() ? html`
    <ntp-action-chips
        ?show-background="${this.showActionChipsBackground_()}"
        ?inert="${this.showComposebox_}"
        @action-chip-click="${this.onActionChipClick_}"
        @action-chips-disabled="${this.onActionChipsDisabled_}"
        @action-chips-retrieval-state-changed="${this.onActionChipsRetrievalStateChanged_}">
    </ntp-action-chips>
  ` : ''}
  <cr-toast id="undoToast" duration="10000" ?inert="${this.showComposebox_}">
    <div id="undoToastMessage">${this.undoToastMessage_}</div>
    <cr-button id="undoButton" aria-label="${this.i18n('undoDescription')}"
        @click="${this.onUndoButtonClick_}">
      ${this.i18n('undo')}
    </cr-button>
  </cr-toast>
  ${this.showVoiceSearch_ ? html`
    <dialog id="voiceSearchDialog" tabindex="-1" autofocus
        @close="${this.onVoiceSearchClose_}"
        @click="${this.onVoiceSearchDialogClick_}">
      <div id="voiceSearchCardContainer">
        ${this.showVoiceSearchGlow_() ? html`
          <search-animated-glow id="voiceSearchGlow"
              .coloredTicTacVoiceAnimationEnabled="${true}"
              .isListening="${this.voiceSearchListening_}"
              .requiresVoice="${true}"
              .transcript="${this.voiceSearchTranscript_}"
              .receivedSpeech="${this.voiceSearchReceivedSpeech_}">
          </search-animated-glow>
        ` : ''}
        <cr-composebox-voice-search id="voiceSearch"
            in-voice-search-mode
            .audioWaveEnabled="${this.voiceSearchHelperTextEnabled_}"
            @voice-permission-changed="${this.onVoicePermissionChanged_}"
            @voice-search-cancel="${this.onVoiceSearchCancel_}"
            @voice-search-final-result="${this.onVoiceSearchFinalResult_}"
            @voice-search-error="${this.onVoiceSearchError_}"
            @voice-search-restart="${this.onVoiceSearchRestart_}"
            @transcript-update="${this.onVoiceSearchTranscriptUpdate_}"
            @speech-received="${this.onVoiceSearchSpeechReceived_}"
            @recording-stopped="${this.onVoiceSearchRecordingStopped_}"
            .hasErrorTimer="${true}"
            .pageCallbackRouter="${this.searchboxCallbackRouter_}"
            .autosubmitEnabled="${this.voiceSearchAutoEndpointEnabled_}"
            .dynamicTimeoutEnabled="${false}"
            .androidContinuousRecognitionEnabled="${
              this.androidSpeechRecognition_}"
            .queryLengthLimit="${this.voiceQueryLengthLimit_}"
            .idleTimeout="${this.voiceAutoSubmitIdleTimeoutMs_}"
            .manualSubmitIdleTimeout="${this.voiceManualSubmitIdleTimeoutMs_}"
            .liveTranscriptEnabled="${this.voiceSearchLiveTranscriptionEnabled_}"
            .helperTextEnabled="${this.voiceSearchHelperTextEnabled_}"
            .submitStopButtonsEnabled="${true}"
            .metricSource="${'NTP_REALBOX'}">
        </cr-composebox-voice-search>
      </div>
    </dialog>
  ` : ''}
</div>
${!this.ntpRealboxNextEnabled_ &&
    (this.showComposebox_ || this.showVoiceSearch_) ? html`
  <div id="scrim" @click="${this.onScrimClick_}"></div>
` : ''}`;
}
