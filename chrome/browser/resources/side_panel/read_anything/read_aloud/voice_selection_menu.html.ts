// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {VoiceSelectionMenuElement} from './voice_selection_menu.js';

export function getHtml(this: VoiceSelectionMenuElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-lazy-render-lit
  id="voiceSelectionMenu"
  .template='${() => html`
    <cr-action-menu
        @close="${this.onClose_}"
        @keydown="${this.onVoiceMenuKeyDown_}"
        accessibility-label="$i18n{voiceSelectionLabel}"
        role-description="$i18n{menu}"
    >
      ${this.errorMessages_.map((item) => html`
        <p class="dropdown-line notification error-message">${item}</p>
      `)}
      ${this.downloadingMessages_.map((item) => html`
        <span class="dropdown-line notification download-message">${item}</span>
      `)}

      ${this.voiceGroups_.map((voiceGroup, groupIndex) => html`
        <span class="dropdown-line lang-group-title">
          ${voiceGroup.language}
        </span>

        ${voiceGroup.voices.map((voice, voiceIndex) => html`
            <button data-test-id="${voice.id}"
                tabindex="${this.voiceItemTabIndex_(groupIndex, voiceIndex)}"
                class="dropdown-item dropdown-voice-selection-button"
                data-group-index="${groupIndex}"
                data-voice-index="${voiceIndex}"
                aria-label="${this.voiceLabel_(voice.selected, voice.title)}"
                @click="${this.onVoiceSelectClick_}">
              <span class="voice-name">
                <cr-icon id="check-mark"
                    class="item-hidden-${!voice.selected} check-mark"
                    icon="read-anything-20:check-mark">
                </cr-icon>
                ${voice.title}
              </span>

              <span id="spinner-span"
                  class="item-hidden-${this.hideSpinner_(voice)}">
                <picture class="spinner">
                  <source media="(prefers-color-scheme: dark)"
                      srcset="//resources/images/throbber_small_dark.svg">
                  <img srcset="//resources/images/throbber_small.svg" alt="">
                </picture>
              </span>

              <cr-icon-button id="preview-icon"
                  tabindex="${this.voiceItemTabIndex_(groupIndex, voiceIndex)}"
                  aria-disabled="${this.shouldDisableButton_(voice)}"
                  class="clickable-${!this.shouldDisableButton_(voice)}"
                  @click="${this.onVoicePreviewClick_}"
                  data-group-index="${groupIndex}"
                  data-voice-index="${voiceIndex}"
                  title="${this.previewLabel_(voice.previewInitiated)}"
                  iron-icon="${this.previewIcon_(voice.previewInitiated)}">
              </cr-icon-button>
            </button>
        `)}
      `)}

      <hr class="sp-hr">
      <button
          class="dropdown-item dropdown-voice-selection-button language-menu-button"
          tabindex="0"
          @click="${this.openLanguageMenu_}">
        $i18n{readingModeLanguageMenu}
      </button>

    </cr-action-menu>
  `}'>
</cr-lazy-render-lit>

${this.showLanguageMenuDialog_
  ?  html`
  <language-menu id="languageMenu"
      .enabledLangs="${this.enabledLangs}"
      .localeToDisplayName="${this.localeToDisplayName}"
      .selectedLang="${this.selectedVoice?.lang}"
      .availableVoices="${this.availableVoices}"
      @close="${this.onLanguageMenuClose_}">
  </language-menu>
  `
  : ''
}
<!--_html_template_end_-->`;
  // clang-format on
}
