// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ReadAnythingToolbarElement} from './read_anything_toolbar.js';

export function getHtml(this: ReadAnythingToolbarElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="toolbarContainer" class="immersive-toolbar-container"
    role="toolbar" aria-label="$i18n{readingModeReadAloudToolbarLabel}"
    tabindex="0"
    @keydown="${this.onToolbarKeydown_}">
    ${this.isLineFocusShowing ? html`
    <cr-button class="toolbar-button" id="line-focus-off"
      tabindex="-1"
      @click="${this.onLineFocusOffClick_}">
      ${this.i18n('turnLineFocusOffTitle')}
    </cr-button>
    ` : ''}
    <span id="audio-controls">
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
      <span id="granularity-container">
        <cr-icon-button id="previousGranularity"
            class="toolbar-button audio-controls"
            ?disabled="${this.shouldDisableGranularityNavButtons_()}"
            aria-label="$i18n{previousSentenceLabel}"
            title="$i18n{previousSentenceLabel}"
            iron-icon="cr:chevron-left"
            tabindex="-1"
            @click="${this.onPreviousGranularityClick_}">
        </cr-icon-button>
        <cr-icon-button id="nextGranularity"
            class="toolbar-button audio-controls"
            aria-label="$i18n{nextSentenceLabel}"
            ?disabled="${this.shouldDisableGranularityNavButtons_()}"
            title="$i18n{nextSentenceLabel}"
            iron-icon="cr:chevron-right"
            tabindex="-1"
            @click="${this.onNextGranularityClick_}">
        </cr-icon-button>
      </span>
      <cr-button class="toolbar-button" id="rate"
          tabindex="${this.getRateTabIndex_()}"
          aria-label="${this.getVoiceSpeedLabel_()}"
          title="${this.i18n('voiceSpeedLabel')}"
          aria-haspopup="menu"
          @click="${this.onShowRateMenuClick_}">
          ${this.getFormattedSpeechRate_()}
      </cr-button>
    </span>
    ${this.isAiPlaybackUiEnabled_ ? html`
    <cr-icon-button class="toolbar-button ${this.isAiPlaybackActive ? 'active'
        : ''}"
        id="ai-playback-toggle"
        aria-label="${this.isAiPlaybackActive
            ? this.i18n('aiPlaybackTurnOff')
            : this.i18n('aiPlaybackTurnOn')}"
        title="${this.isAiPlaybackActive
            ? this.i18n('aiPlaybackTurnOff')
            : this.i18n('aiPlaybackTurnOn')}"
        iron-icon="read-anything:audio_magic_eraser"
        @click="${this.onAiPlaybackClick_}">
    </cr-icon-button>
    ` : ''}

  ${this.textStyleOptions_.map((item, index) => html`
    ${item.announceId ? html`
      <div id="${item.announceId}" class="announce-block" aria-live="polite">
      </div>
    ` : ''}
    <cr-icon-button class="toolbar-button text-style-button"
        id="${item.id}"
        tabindex="-1"
        data-index="${index}"
        aria-label="${item.ariaLabel}"
        title="${item.ariaLabel}"
        aria-haspopup="menu"
        iron-icon="${item.icon}"
        @click="${this.onTextStyleMenuButtonClick_}">
    </cr-icon-button>
  `)}
  <cr-icon-button id="more" tabindex="-1" aria-label="$i18n{settingsLabel}"
      class="toolbar-button"
      title="$i18n{settingsLabel}"
      aria-haspopup="menu"
      iron-icon="${this.webuiRoundedIconsEnabled_
          ? 'read-anything:settings'
          : 'read-anything:settings-old'}"
      @click="${this.onMoreOptionsClick_}">
  </cr-icon-button>
  ${this.isImmersiveMode ? html`
    <cr-icon-button id="close" tabindex="-1"
        class="toolbar-button"
        aria-label="$i18n{readingModeClose}"
        title="$i18n{readingModeClose}"
        iron-icon="cr:close"
        @click="${this.onCloseClick_}">
    </cr-icon-button>
  ` : ''}
  <settings-menu
    id="settingsMenu"
    .settingsPrefs="${this.settingsPrefs}"
    .isImmersiveMode="${this.isImmersiveMode}"
    .isReadAnythingPinned="${this.isReadAnythingPinned}"
    .isSpeechActive="${this.isSpeechActive}"
    @close-submenu-requested="${this.onCloseSubmenuRequested_}"
    @close-all-menus="${this.onCloseAllMenus_}"
    @open-settings-submenu="${this.onOpenSettingsSubmenu_}"
    @translation-requested="${this.onTranslationRequested_}">
  </settings-menu>
  <presentation-menu id="presentationMenu"
    class="settings-submenu"
    .presentationState="${this.presentationState}"
    @close-all-menus="${this.onCloseAllMenus_}">
  </presentation-menu>
  <cr-lazy-render-lit id="fontSizeMenu" .template='${() => html`
  <cr-action-menu @keydown="${this.onFontSizeMenuKeydown_}"
      accessibility-label="$i18n{fontSizeTitle}"
      role-description="$i18n{menu}"
      class="immersive-font-size-menu">
    <cr-icon-button class="font-size" role="menuitem"
        id="font-size-decrease"
        aria-label="$i18n{decreaseFontSizeLabel}"
        title="$i18n{decreaseFontSizeLabel}"
        iron-icon="${this.webuiRoundedIconsEnabled_
            ? 'read-anything:remove'
            : 'read-anything:font-size-decrease-old'}"
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
        ?disabled="${this.isFontSizeDefault_()}"
        aria-label="$i18n{fontResetTooltip}"
        title="$i18n{fontResetTooltip}"
        @click="${this.onFontResetClick_}">
      $i18n{fontResetTitle}
    </cr-button>
  </cr-action-menu>
  `}'>
  </cr-lazy-render-lit>
  <rate-menu id="rateMenu" .settingsPrefs="${this.settingsPrefs}"
    @rate-change="${this.onRateChange_}">
  </rate-menu>
  <highlight-menu
    id="highlightMenu"
    class="settings-submenu"
    .nonModal="${true}"
    .settingsPrefs="${this.settingsPrefs}"
    @close-all-menus="${this.onCloseAllMenus_}">
  </highlight-menu>
  <color-menu
      id="colorMenu"
      class="settings-submenu"
      .nonModal="${true}"
      .settingsPrefs="${this.settingsPrefs}"
      @close-all-menus="${this.onCloseAllMenus_}">
  </color-menu>
  <line-spacing-menu
      id="lineSpacingMenu"
      class="settings-submenu"
      .nonModal="${true}"
      .settingsPrefs="${this.settingsPrefs}"
      @close-all-menus="${this.onCloseAllMenus_}">
  </line-spacing-menu>
  <letter-spacing-menu
      id="letterSpacingMenu"
      class="settings-submenu"
      .nonModal="${true}"
      .settingsPrefs="${this.settingsPrefs}"
      @close-all-menus="${this.onCloseAllMenus_}">
  </letter-spacing-menu>
  <font-menu
      id="fontMenu"
      class="settings-submenu"
      .nonModal="${true}"
      .areFontsLoaded="${this.areFontsLoaded_}"
      .settingsPrefs="${this.settingsPrefs}"
      .pageLanguage="${this.pageLanguage}"
      @font-change="${this.onFontChange_}"
      @close-all-menus="${this.onCloseAllMenus_}">
  </font-menu>
  <line-focus-menu
      id="lineFocusMenu"
      class="settings-submenu"
      .nonModal="${true}"
      .settingsPrefs="${this.settingsPrefs}"
      .lineFocusStyle="${this.lineFocusStyle}"
      .lineFocusEnabled="${this.lineFocusEnabled}"
      .lineFocusMovement="${this.lineFocusMovement}"
      @close-all-menus="${this.onCloseAllMenus_}">
  </line-focus-menu>
  <appearance-menu
      id="appearanceMenu"
      class="settings-submenu"
      non-modal
      .settingsPrefs="${this.settingsPrefs}"
      .presentationState="${this.presentationState}"
      @close-all-menus="${this.onCloseAllMenus_}">
  </appearance-menu>
  <audio-menu
      id="audioMenu"
      class="settings-submenu"
      non-modal
      .settingsPrefs="${this.settingsPrefs}"
      .enabledLangs="${this.enabledLangs}"
      .availableVoices="${this.availableVoices}"
      .localeToDisplayName="${this.localeToDisplayName}"
      .selectedLang="${this.selectedVoice?.lang || ''}"
      @close-all-menus="${this.onCloseAllMenus_}">
  </audio-menu>
  <text-menu
      id="textMenu"
      class="settings-submenu"
      non-modal
      .settingsPrefs="${this.settingsPrefs}"
      .areFontsLoaded="${this.areFontsLoaded_}"
      .pageLanguage="${this.pageLanguage}"
      @close-all-menus="${this.onCloseAllMenus_}">
  </text-menu>
  <media-menu
      id="mediaMenu"
      class="settings-submenu"
      non-modal
      .settingsPrefs="${this.settingsPrefs}"
      .isSpeechActive="${this.isSpeechActive}"
      @close-all-menus="${this.onCloseAllMenus_}">
  </media-menu>
  <voice-selection-menu id="voiceSelectionMenu"
      class="settings-submenu"
      .nonModal="${true}"
      .selectedVoice="${this.selectedVoice}"
      .availableVoices="${this.availableVoices}"
      .enabledLangs="${this.enabledLangs}"
      .localeToDisplayName="${this.localeToDisplayName}"
      .previewVoicePlaying="${this.previewVoicePlaying}"
      @close-all-menus="${this.onCloseAllMenus_}">
  </voice-selection-menu>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
