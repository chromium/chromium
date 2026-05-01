// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OmniboxPopupSearchboxElement} from './omnibox_popup_searchbox.js';

export function getHtml(this: OmniboxPopupSearchboxElement) {
  return html`
    <div id="inputWrapper" @focusout="${this.onInputWrapperFocusout}"
        @keydown="${this.onInputWrapperKeydown}">
      <cr-searchbox-input id="input"
          exportparts="searchbox-input"
          ?dropdown-is-visible="${this.dropdownIsVisible}"
          input-aria-live="${this.inputAriaLive}"
          ?multi-line-enabled="${this.multiLineEnabled}"
          placeholder-text="${this.computePlaceholderText_()}"
          searchbox-aria-description="${this.searchboxAriaDescription}"
          searchbox-icon="${this.searchboxIcon_}"
          .selectedMatch="${this.selectedMatch}"
          ?input-has-matches="${this.hasMatches()}"
          @focusin="${this.onInputFocusin_}"
          @searchbox-input-text-updated="${this.onSearchboxInputTextUpdated_}"
          @input-focus-changed="${this.onInputFocusChanged}">
        ${this.shouldShowVoiceLens_(this.searchboxVoiceSearchEnabled_) ? html`
          <div slot="action-buttons"
              class="searchbox-icon-button-container voice">
            <button id="voiceSearchButton" class="searchbox-icon-button"
                @click="${this.onVoiceSearchClick_}"
                title="${this.i18n('voiceSearchButtonLabel')}">
            </button>
          </div>
        `: ''}
        ${this.shouldShowVoiceLens_(this.searchboxLensSearchEnabled_) ? html`
          <div slot="action-buttons"
              class="searchbox-icon-button-container lens">
            <button id="lensSearchButton" class="searchbox-icon-button"
                @click="${this.onLensSearchClick_}"
                title="${this.i18n('lensSearchButtonLabel')}">
            </button>
          </div>
        ` : ''}
      </cr-searchbox-input>
      <div class="dropdownContainer">
        <cr-searchbox-dropdown id="matches" part="searchbox-dropdown"
            exportparts="dropdown-content"
            role="listbox" .result="${this.result}"
            .selectedMatchIndex="${this.selectedMatchIndex}"
            @selected-match-index-changed="${this.onSelectedMatchIndexChanged}"
            @match-focusin="${this.onMatchFocusin}"
            @match-click="${this.onMatchClick}"
            ?hidden="${!this.dropdownIsVisible}">
        </cr-searchbox-dropdown>
      </div>
    </div>
  `;
}
