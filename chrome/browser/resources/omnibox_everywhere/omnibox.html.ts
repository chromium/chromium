// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OmniboxEverywhereOmniboxElement} from './omnibox.js';

export function getHtml(this: OmniboxEverywhereOmniboxElement) {
  return html`
    <div id="inputWrapper" @focusout="${this.onInputWrapperFocusout}"
        @keydown="${this.onInputWrapperKeydown}">
      <search-animated-glow
        animation-state="${this.animationState_}"
        part="animated-glow">
      </search-animated-glow>
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
        <div class="contextualEntrypointContainer
                    contextualEntrypointContainerCompact"
             slot="contextual-entrypoint">
          <cr-composebox-file-inputs>
            <div class="context-menu-container" id="contextMenuContainer">
              <cr-composebox-contextual-entrypoint-and-menu id="context"
                  exportparts="context-menu-entrypoint-icon"
                  class="upload-button"
                  disable-auto-reposition
                  glif-animation-state="${this.contextMenuGlifAnimationState}"
                  .inputState="${this.inputState_}"
                  .searchboxLayoutMode="${this.searchboxLayoutMode}"
                  .tabSuggestions="${this.tabSuggestions_}"
                  .tabSuggestionsLoading="${this.tabSuggestionsLoading_}"
                  .tabSuggestionsHasLoaded="${this.tabSuggestionsHasLoaded_}"
                  @context-menu-entrypoint-click="${
      this.onContextMenuEntrypointClick_}"
                  @context-menu-opened="${this.onContextMenuOpened_}"
                  @context-menu-closed="${this.onContextMenuClosed_}"
                  @request-tab-suggestions-load="${
      this.onRequestTabSuggestionsLoad}"
                  @tool-click="${this.onToolClick_}"
                  @deep-search-click="${this.onDeepSearchClick_}"
                  @create-image-click="${this.onCreateImageClick_}"
                  @model-click="${this.onModelClick_}">
              </cr-composebox-contextual-entrypoint-and-menu>
            </div>
          </cr-composebox-file-inputs>
        </div>
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
        ${
      this.composeButtonEnabled ? html`
          <cr-searchbox-compose-button id="composeButton" slot="compose-button"
              @compose-click="${this.onComposeClick_}">
          </cr-searchbox-compose-button>
        ` :
                                  ''}
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
