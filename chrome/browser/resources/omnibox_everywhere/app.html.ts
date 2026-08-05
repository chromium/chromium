// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OmniboxEverywhereAppElement} from './app.js';

export function getHtml(this: OmniboxEverywhereAppElement) {
  return html`<!--_html_template_start_-->
<div id="content">
  ${
      this.isComposeboxMode_ ? html`
    <omnibox-everywhere-composebox id="composebox" searchbox-next-enabled
        searchbox-layout-mode="${this.searchboxLayoutMode_}"
        .state="${this.composeboxState_}"
        @close-composebox="${this.onCloseComposebox_}"
        @composebox-submit="${this.onComposeboxSubmit_}"
        @open-voice-search="${this.onOpenVoiceSearch_}"
        .inVoiceSearchMode="${this.showVoiceSearchOverlay_}"
        .showVoiceSearch="${true}"
        .usePecApi="${this.usePecApi_}"
        .isOblongShape="${this.isOblongShape_}"
        .contextManagementInComposeboxEnabled="${
                                   this.contextManagementInComposeboxEnabled_}"
        entrypoint-name="Omnibox">
    </omnibox-everywhere-composebox>
  ` :
                               html`
    <omnibox-everywhere-omnibox id="searchbox"
        @open-composebox="${this.onOpenComposebox_}"
        @open-voice-search="${this.onOpenVoiceSearch_}"
        .inVoiceSearchMode="${this.showVoiceSearchOverlay_}"
        .contextManagementInComposeboxEnabled="${
                                   this.contextManagementInComposeboxEnabled_}">
    </omnibox-everywhere-omnibox>
  `}
</div>
<div id="dialogAnchor"></div>
${this.showVoiceSearchOverlay_ ? html`
  <dialog id="voiceSearchDialog" tabindex="-1" autofocus
      @close="${this.onVoiceSearchOverlayClose_}"
      @click="${this.onVoiceSearchDialogClick_}">
    <div id="voiceSearchCardContainer">
      ${!this.hasVoiceSearchError_ ? html`
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
          @voice-permission-changed="${this.onVoicePermissionChanged_}"
          @voice-search-cancel="${this.onVoiceSearchCancel_}"
          @voice-search-final-result="${this.onVoiceSearchFinalResult_}"
          @voice-search-error="${this.onVoiceSearchError_}"
          @voice-search-restart="${this.onVoiceSearchRestart_}"
          @transcript-update="${this.onVoiceSearchTranscriptUpdate_}"
          @speech-received="${this.onVoiceSearchSpeechReceived_}"
          @recording-stopped="${this.onVoiceSearchRecordingStopped_}"
          .hasErrorTimer="${true}"
          .idleTimeout="${this.voiceIdleTimeoutMs_}"
          .dynamicTimeoutEnabled="${true}"
          .queryLengthLimit="${this.voiceQueryLengthLimit_}"
          .liveTranscriptEnabled="${false}"
          .pageCallbackRouter="${this.callbackRouter_}"
          .autosubmitEnabled="${true}"
          .submitStopButtonsEnabled="${true}">
      </cr-composebox-voice-search>
    </div>
  </dialog>
` : ''}
<!--_html_template_end_-->`;
}
