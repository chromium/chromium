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
        .pageLanguage="${this.pageLanguage_}"
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
  <div id="containerParent" class="sp-card"
      ?hidden="${!this.computeHasContent()}">
    <div id="containerScroller" class="sp-scroller"
        @scroll="${this.onContainerScroll_}"
        @scrollend="${this.onContainerScrollEnd_}">
      <div id="container"
        class=
          "user-select-disabled-when-speech-active-${this.isSpeechActive_}">
      </div>
    </div>
    <!-- TODO: crbug.com/324143642- Localize the "Load More" string. -->
    <cr-button id="docs-load-more-button" tabindex="0"
        @click="${this.onDocsLoadMoreButtonClick_}"
        ?hidden="${!this.isDocsLoadMoreButtonVisible_}">
      Load More
    </cr-button>
  </div>
  <div id="empty-state-container" ?hidden="${this.computeHasContent()}">
    <sp-empty-state image-path="${this.contentState_.imagePath}"
        dark-image-path="${this.contentState_.darkImagePath}"
        heading="${this.contentState_.heading}"
        body="${this.contentState_.subheading}">
    </sp-empty-state>
  </div>
</div>
<language-toast id="languageToast" show-errors
    .numAvailableVoices="${this.availableVoices_.length}">
</language-toast>
<!--_html_template_end_-->`;
  // clang-format on
}
