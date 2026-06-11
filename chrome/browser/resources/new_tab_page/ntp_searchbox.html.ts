// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/searchbox/searchbox_input.js';

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {NtpSearchboxElement} from './ntp_searchbox.js';
import {getHtml as getContextualEntrypointHtml} from './ntp_searchbox_contextual_entrypoint.html.js';

export function getHtml(this: NtpSearchboxElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="inputWrapper" @focusout="${this.onInputWrapperFocusout}"
    @keydown="${this.onInputWrapperKeydown}"
    @dragenter="${this.dragAndDropHandler?.handleDragEnter || nothing}"
    @dragover="${this.dragAndDropHandler?.handleDragOver || nothing}"
    @dragleave="${this.dragAndDropHandler?.handleDragLeave || nothing}"
    @drop="${this.dragAndDropHandler?.handleDrop || nothing}">
  ${this.ntpRealboxNextEnabled ?
    html`
      <search-animated-glow
        animation-state="${this.animationState}"
        .isListening="${this.isListening}"
        .darkThemeColorsEnabled="${false}"
        part="animated-glow">
      </search-animated-glow>
    ` : ''}
  <cr-searchbox-input id="input"
      exportparts="searchbox-input"
      ?dropdown-is-visible="${this.dropdownIsVisible}"
      input-aria-live="${this.inputAriaLive}"
      ?multi-line-enabled="${this.multiLineEnabled}"
      placeholder-text="${this.computePlaceholderText_(this.placeholderText)}"
      searchbox-aria-description="${this.searchboxAriaDescription}"
      searchbox-icon="${this.searchboxIcon_}"
      .selectedMatch="${this.selectedMatch}"
      ?input-has-matches="${this.inputHasMatches_()}"
      ?allow-file-paste="${this.ntpRealboxNextEnabled}"
      @focusin="${this.onInputFocusin_}"
      @searchbox-input-files-pasted="${this.onSearchboxInputFilesPasted_}"
      @searchbox-input-text-updated="${this.onSearchboxInputTextUpdated_}"
      @input-focus-changed="${this.onInputFocusChanged}">
    ${this.ntpRealboxNextEnabled ? html`
      <div class="contextualEntrypointContainer contextualEntrypointContainerCompact" slot="contextual-entrypoint">
        ${getContextualEntrypointHtml.bind(this)()}
      </div>
    ` : ''}
    ${this.shouldShowVoiceLens_(this.searchboxVoiceSearchEnabled_) ? html`
      <div slot="action-buttons" class="searchbox-icon-button-container voice">
        <button id="voiceSearchButton" class="searchbox-icon-button"
            @click="${this.onVoiceSearchClick}"
            title="${this.i18n('voiceSearchButtonLabel')}">
        </button>
      </div>
    ` : ''}
    ${this.shouldShowVoiceLens_(this.searchboxLensSearchEnabled_) ? html`
      <div slot="action-buttons" class="searchbox-icon-button-container lens">
        <button id="lensSearchButton" class="searchbox-icon-button lens"
            @click="${this.onLensSearchClick_}"
            title="${this.i18n('lensSearchButtonLabel')}">
        </button>
      </div>
    ` : ''}
    ${this.composeButtonEnabled ? html`
      <cr-searchbox-compose-button id="composeButton" slot="compose-button"
          @compose-click="${this.onComposeClick_}">
      </cr-searchbox-compose-button>
    ` : ''}
  </cr-searchbox-input>
  <div class="dropdownContainer">
    <cr-searchbox-dropdown id="matches" part="searchbox-dropdown"
        exportparts="dropdown-content"
        role="listbox" .result="${this.result}"
        selected-match-index="${this.selectedMatchIndex}"
        @selected-match-index-changed="${this.onSelectedMatchIndexChanged}"
        ?can-show-secondary-side="${this.canShowSecondarySide}"
        ?had-secondary-side="${this.hadSecondarySide}"
        @had-secondary-side-changed="${this.onHadSecondarySideChanged_}"
        ?has-secondary-side="${this.hasSecondarySide}"
        @has-secondary-side-changed="${this.onHasSecondarySideChanged_}"
        @match-focusin="${this.onMatchFocusin}"
        @match-click="${this.onMatchClick}"
        ?hidden="${!this.dropdownIsVisible}">
    </cr-searchbox-dropdown>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
