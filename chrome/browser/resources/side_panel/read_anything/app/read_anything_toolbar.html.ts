// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ReadAnythingToolbarElement} from './read_anything_toolbar.js';

export function getHtml(this: ReadAnythingToolbarElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="toolbarContainer" class="toolbar-container" role="toolbar"
    aria-label="${this.getToolbarAriaLabel_()}"
    @keydown="${this.onToolbarKeyDown_}"
    @reset-toolbar="${this.onResetToolbar_}"
    @toolbar-overflow="${this.onToolbarOverflow_}">
  ${this.isReadAloudEnabled_ ? html`
    <span id="audio-controls"
        class="audio-background-when-active-${this.isSpeechActive}">
      <span ?hidden="${this.hideSpinner_}">
        <picture class="spinner toolbar-button audio-controls">
          <source media="(prefers-color-scheme: dark)"
              srcset="//resources/images/throbber_small_dark.svg">
          <img srcset="//resources/images/throbber_small.svg" alt="">
        </picture>
      </span>

      <cr-icon-button class="toolbar-button audio-controls" id="play-pause"
          ?disabled="${!this.isReadAloudPlayable}"
          title="${this.playPauseButtonTitle_()}"
          aria-label="${this.playPauseButtonAriaLabel_()}"
          aria-keyshortcuts="k"
          aria-description="$i18n{playDescription}"
          iron-icon="${this.playPauseButtonIronIcon_()}"
          tabindex="0"
          @click="${this.onPlayPauseClick_}">
      </cr-icon-button>
      <span id="granularity-container"
          class="granularity-container-when-active-${this.isSpeechActive}">
        <cr-icon-button id="previousGranularity"
            class="toolbar-button audio-controls"
            ?disabled="${!this.isReadAloudPlayable}"
            aria-label="$i18n{previousSentenceLabel}"
            title="$i18n{previousSentenceLabel}"
            iron-icon="cr:chevron-left"
            tabindex="-1"
            @click="${this.onPreviousGranularityClick_}">
        </cr-icon-button>
        <cr-icon-button id="nextGranularity"
            class="toolbar-button audio-controls"
            aria-label="$i18n{nextSentenceLabel}"
            ?disabled="${!this.isReadAloudPlayable}"
            title="$i18n{nextSentenceLabel}"
            iron-icon="cr:chevron-right"
            tabindex="-1"
            @click="${this.onNextGranularityClick_}">
        </cr-icon-button>
      </span>
    </span>
    <cr-button class="toolbar-button" id="rate"
        tabindex="${this.getRateTabIndex_()}"
        aria-label="${this.getVoiceSpeedLabel_()}"
        title="$i18n{voiceSpeedLabel}"
        aria-haspopup="menu"
        @click="${this.onShowRateMenuClick_}">
        ${this.getFormattedSpeechRate_()}
    </cr-button>
    <cr-icon-button class="toolbar-button" id="voice-selection" tabindex="-1"
        aria-label="$i18n{voiceSelectionLabel}"
        title="$i18n{voiceSelectionLabel}"
        aria-haspopup="menu"
        iron-icon="read-anything:voice-selection"
        @click="${this.onVoiceSelectionMenuClick_}">
    </cr-icon-button>
    <voice-selection-menu id="voiceSelectionMenu"
        .selectedVoice="${this.selectedVoice}"
        .availableVoices="${this.availableVoices}"
        .enabledLangs="${this.enabledLangs}"
        .localeToDisplayName="${this.localeToDisplayName}"
        .previewVoicePlaying="${this.previewVoicePlaying}">
    </voice-selection-menu>
    <cr-icon-button class="toolbar-button" id="highlight" tabindex="-1"
        iron-icon="read-anything:highlight-on"
        title="${this.getHighlightButtonLabel_()}"
        aria-label="${this.getHighlightButtonLabel_()}"
        @click="${this.onHighlightClick_}">
    </cr-icon-button>
  `: html`
    <!-- isReadAloudEnabled_ === false -->
    <font-select id="font-select" tabindex="0"
      .settingsPrefs="${this.settingsPrefs}"
      .pageLanguage="${this.pageLanguage}"
      .areFontsLoaded="${this.areFontsLoaded_}"
      @font-change="${this.onFontChange_}">
    </font-select>
    <hr class="separator" aria-hidden="true">
    <div id="size-announce" class="announce-block" aria-live="polite"></div>
    <cr-icon-button id="font-size-decrease-old" tabindex="-1"
        class="toolbar-button"
        aria-label="$i18n{decreaseFontSizeLabel}"
        title="$i18n{decreaseFontSizeLabel}"
        iron-icon="read-anything:font-size-decrease-old"
        @click="${this.onFontSizeDecreaseClick_}">
    </cr-icon-button>
    <cr-icon-button id="font-size-increase-old" tabindex="-1"
        class="toolbar-button"
        aria-label="$i18n{increaseFontSizeLabel}"
        title="$i18n{increaseFontSizeLabel}"
        iron-icon="read-anything:font-size-increase-old"
        @click="${this.onFontSizeIncreaseClick_}">
    </cr-icon-button>
  `}

  <hr class="separator" aria-hidden="true">

  ${this.textStyleToggles_.map((item) => html`
    <cr-icon-button tabindex="-1" class="toolbar-button"
        ?disabled="${this.isSpeechActive}"
        id="${item.id}"
        aria-label="${item.title}"
        title="${item.title}"
        iron-icon="${item.icon}"
        @click="${this.onToggleButtonClick_}">
    </cr-icon-button>
  `)}

  ${this.textStyleOptions_.map((item, index) => html`
    ${item.announceBlock??html``}
    <cr-icon-button class="toolbar-button text-style-button" id="${item.id}"
        tabindex="-1"
        data-index="${index}"
        aria-label="${item.ariaLabel}"
        title="${item.ariaLabel}"
        aria-haspopup="menu"
        iron-icon="${item.icon}"
        @click="${this.onTextStyleMenuButtonClick_}">
    </cr-icon-button>
  `)}
  <cr-icon-button id="more" tabindex="-1" aria-label="$i18n{moreOptionsLabel}"
      class="hidden"
      title="$i18n{moreOptionsLabel}"
      aria-haspopup="menu"
      iron-icon="cr:more-vert"
      @click="${this.onMoreOptionsClick_}">
  </cr-icon-button>

  <cr-lazy-render-lit id="moreOptionsMenu" .template='${() => html`
    <cr-action-menu id="more-options-menu-dialog"
        @keydown="${this.onToolbarKeyDown_}"
        role-description="$i18n{menu}">
      ${this.moreOptionsButtons_.map((item, index) => html`
        <cr-icon-button id="${item.id}" class="more-options-icon"
            aria-label="${item.ariaLabel}"
            data-index="${index}"
            title="${item.ariaLabel}"
            aria-haspopup="menu"
            iron-icon="${item.icon}"
            @click="${this.onTextStyleMenuButtonClickFromOverflow_}">
        </cr-icon-button>
      `)}
    </cr-action-menu>
  `}'>
  </cr-lazy-render-lit>
  <rate-menu
      id="rateMenu"
      .settingsPrefs="${this.settingsPrefs}"
      @rate-change="${this.onRateChange_}">
  </rate-menu>
  <highlight-menu
      id="highlightMenu"
      .settingsPrefs="${this.settingsPrefs}"
      @highlight-change="${this.onHighlightChange_}">
  </highlight-menu>
  <cr-lazy-render-lit id="fontSizeMenu" .template='${() => html`
    <cr-action-menu @keydown="${this.onFontSizeMenuKeyDown_}"
        accessibility-label="$i18n{fontSizeTitle}"
        role-description="$i18n{menu}">
      <cr-icon-button class="font-size" role="menuitem"
          id="font-size-decrease"
          aria-label="$i18n{decreaseFontSizeLabel}"
          title="$i18n{decreaseFontSizeLabel}"
          iron-icon="read-anything:font-size-decrease"
          @click="${this.onFontSizeDecreaseClick_}">
      </cr-icon-button>
      <cr-icon-button class="font-size" role="menuitem"
          id="font-size-increase"
          aria-label="$i18n{increaseFontSizeLabel}"
          title="$i18n{increaseFontSizeLabel}"
          iron-icon="cr:add"
          @click="${this.onFontSizeIncreaseClick_}">
      </cr-icon-button>
      <cr-button role="menuitem"
          id="font-size-reset"
          aria-label="$i18n{fontResetTooltip}"
          title="$i18n{fontResetTooltip}"
          @click="${this.onFontResetClick_}">
        $i18n{fontResetTitle}
      </cr-button>
    </cr-action-menu>
  `}'>
  </cr-lazy-render-lit>
  <color-menu id="colorMenu" .settingsPrefs="${this.settingsPrefs}">
  </color-menu>
  <line-spacing-menu
      id="lineSpacingMenu"
      .settingsPrefs="${this.settingsPrefs}">
  </line-spacing-menu>
  <letter-spacing-menu
      id="letterSpacingMenu"
      .settingsPrefs="${this.settingsPrefs}">
  </letter-spacing-menu>
  <font-menu
      id="fontMenu"
      .areFontsLoaded="${this.areFontsLoaded_}"
      .settingsPrefs="${this.settingsPrefs}"
      .pageLanguage="${this.pageLanguage}"
      @font-change="${this.onFontChange_}">
  </font-menu>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
