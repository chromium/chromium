// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OmniboxPopupSearchboxElement} from './omnibox_popup_searchbox.js';

export function getHtml(this: OmniboxPopupSearchboxElement) {
  // clang-format off
  return html`
    <div id="inputWrapper" @focusout="${this.onInputWrapperFocusout}"
        @keydown="${this.onInputWrapperKeydown}">
      <cr-searchbox-input id="input"
          exportparts="searchbox-input"
          ?dropdown-is-visible="${this.dropdownIsVisible}"
          input-aria-live="${this.inputAriaLive}"
          ?multi-line-enabled="${this.multiLineEnabled}"
          placeholder-text=""
          searchbox-aria-description="${this.searchboxAriaDescription}"
          searchbox-icon="${this.searchboxIcon_}"
          .selectedMatch="${this.selectedMatch}"
          ?input-has-matches="${this.hasMatches()}"
          @focusin="${this.onInputFocusin_}"
          @mousedown="${this.onInputMousedown_}"
          @searchbox-input-text-updated="${this.onSearchboxInputTextUpdated_}"
          @input-focus-changed="${this.onInputFocusChanged}"
          @input-keydown="${this.onInputKeydown_}"
          @paste="${this.onInputPaste_}">
        ${
      this.shouldShowVoiceLens_(this.searchboxVoiceSearchEnabled_) ? html`
          <div slot="action-buttons"
              class="searchbox-icon-button-container voice">
            <button id="voiceSearchButton" class="searchbox-icon-button"
                @click="${this.onVoiceSearchClick_}"
                title="${this.i18n('voiceSearchButtonLabel')}">
            </button>
          </div>
        ` :
                                                                     ''}
        ${
      this.shouldShowVoiceLens_(this.searchboxLensSearchEnabled_) ? html`
          <div slot="action-buttons"
              class="searchbox-icon-button-container lens">
            <button id="lensSearchButton" class="searchbox-icon-button"
                @click="${this.onLensSearchClick_}"
                title="${this.i18n('lensSearchButtonLabel')}">
            </button>
          </div>
        ` :
                                                                    ''}
        <cr-searchbox-compose-button id="composeButton" slot="compose-button"
            ?dynamic="${this.searchboxDynamicAnimation_}"
            ?has-user-input="${this.hasUserInput_}"
            ?hidden="${!this.aimButtonVisible_}"
            label-text="${this.aimButtonConfig_.text}"
            tooltip-title="${this.aimButtonConfig_.title}"
            a11y-label="${this.aimButtonConfig_.a11yLabel}"
            compose-icon="${this.aimButtonConfig_.icon}"
            @compose-click="${this.onComposeClick_}">
        </cr-searchbox-compose-button>
      </cr-searchbox-input>
      <div class="dropdownContainer">
        <cr-searchbox-dropdown id="matches" part="searchbox-dropdown"
            exportparts="dropdown-content"
            role="listbox" .result="${this.result}"
            .selectedMatchIndex="${this.selectedMatchIndex}"
            ?can-show-secondary-side="${this.canShowSecondarySide}"
            ?has-secondary-side="${this.hasSecondarySide}"
            @has-secondary-side-changed="${this.onHasSecondarySideChanged_}"
            @selected-match-index-changed="${this.onSelectedMatchIndexChanged}"
            @match-focusin="${this.onMatchFocusin}"
            @match-click="${this.onMatchClick}"
            ?hidden="${!this.dropdownIsVisible}">
        </cr-searchbox-dropdown>
      </div>
    </div>
  `;
  // clang-format on
}
