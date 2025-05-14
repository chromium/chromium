// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AppElement} from './app.js';

export function getHtml(this: AppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="appFlexParent" @keydown="${this.onKeyDown_}">
  <div id="toolbar-container">
    <read-anything-toolbar
        .isSpeechActive="${this.isSpeechActive_}"
        .isAudioCurrentlyPlaying="${this.isAudioCurrentlyPlaying_}"
        .isReadAloudPlayable="${this.computeIsReadAloudPlayable()}"
        .selectedVoice="${this.selectedVoice_}"
        .settingsPrefs="${this.settingsPrefs_}"
        .enabledLangs="${this.enabledLangs_}"
        .availableVoices="${this.availableVoices_}"
        .previewVoicePlaying="${this.previewVoicePlaying_}"
        .localeToDisplayName="${this.localeToDisplayName_}"
        @select-voice="${this.onSelectVoice_}"
        @voice-language-toggle="${this.onVoiceLanguageToggle_}"
        @preview-voice="${this.onPreviewVoice_}"
        @voice-menu-close="${this.onVoiceMenuClose_}"
        @voice-menu-open="${this.onVoiceMenuOpen_}"
        @play-pause-click="${this.onPlayPauseClick_}"
        @font-size-change="${this.onFontSizeChange_}"
        @font-change="${this.onFontChange_}"
        @rate-change="${this.onSpeechRateChange_}"
        @next-granularity-click="${this.onNextGranularityClick_}"
        @previous-granularity-click="${this.onPreviousGranularityClick_}"
        @links-toggle="${this.updateLinks_}"
        @images-toggle="${this.updateImages_}"
        @letter-spacing-change="${this.onLetterSpacingChange_}"
        @theme-change="${this.onThemeChange_}"
        @line-spacing-change="${this.onLineSpacingChange_}"
        @highlight-change="${this.onHighlightChange_}"
        @reset-toolbar="${this.onResetToolbar_}"
        @toolbar-overflow="${this.onToolbarOverflow_}"
        @language-menu-open="${this.onLanguageMenuOpen_}"
        @language-menu-close="${this.onLanguageMenuClose_}"
        id="toolbar">
    </read-anything-toolbar>
  </div>
  <div id="containerParent" class="sp-card sp-scroller"
      ?hidden="${!this.hasContent_}">
    <div id="container"
        class="user-select-disabled-when-speech-active-${this.isSpeechActive_}">
    </div>
    <cr-button id="docs-load-more-button" tabindex="0"
        @click="${this.onDocsLoadMoreButtonClick_}"
        ?hidden="${!this.isDocsLoadMoreButtonVisible_}">
        Load More
    </cr-button>
  </div>
  <div id="empty-state-container" ?hidden="${this.hasContent_}">
    <sp-empty-state image-path="${this.emptyStateImagePath_}"
        dark-image-path="${this.emptyStateDarkImagePath_}"
        heading="${this.emptyStateHeading_}"
        body="${this.emptyStateSubheading_}">
    </sp-empty-state>
  </div>
</div>
<language-toast id="languageToast" show-errors
    .numAvailableVoices="${this.availableVoices_.length}">
</language-toast>
<!--_html_template_end_-->`;
  // clang-format on
}
