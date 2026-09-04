// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OmniboxEverywhereOmniboxElement} from './omnibox.js';

export function getHtml(this: OmniboxEverywhereOmniboxElement) {
  return html`
    <div id="inputWrapper" @focusout="${this.onInputWrapperFocusout}"
        @keydown="${this.onInputWrapperKeydown}"
        @dragenter="${this.dragAndDropHandler.handleDragEnter}"
        @dragover="${this.dragAndDropHandler.handleDragOver}"
        @dragleave="${this.dragAndDropHandler.handleDragLeave}"
        @drop="${this.dragAndDropHandler.handleDrop}">
      <search-animated-glow
        .animationState="${this.animationState}"
        .energyEffectAnimationEnabled="${this.energyEffectAnimationEnabled_}"
        .entrypointName="${this.entrypointName}"
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
          .inputKeywordModel="${this.inputKeywordModel}"
          ?input-has-matches="${this.hasMatches()}"
          ?allow-file-paste="${this.fileContextEnabled_}"
          @focusin="${this.onInputFocusin_}"
          @searchbox-input-files-pasted="${this.onSearchboxInputFilesPasted_}"
          @searchbox-input-text-updated="${this.onSearchboxInputTextUpdated_}"
          @input-focus-changed="${this.onInputFocusChanged}">
        ${
      this.composeButtonEnabled ? html`
          <cr-searchbox-compose-button id="composeButton" slot="compose-button"
              ?dynamic="${this.ntpRealboxDynamicAiModeButtonEnabled_}"
              ?has-user-input="${this.hasUserInput_}"
              ?virtual-focus-enabled="${this.virtualFocusEnabled}"
              ?has-virtual-focus="${this.isAiModeVirtualFocused()}"
              ?dropdown-is-visible="${this.dropdownIsVisible}"
              @compose-click="${this.onComposeClick_}">
          </cr-searchbox-compose-button>
        ` :
                                  ''}
      </cr-searchbox-input>
      <omnibox-everywhere-profile-icon id="profileIcon"></omnibox-everywhere-profile-icon>
      <div class="dropdownContainer">
        <cr-searchbox-dropdown id="matches" part="searchbox-dropdown"
            exportparts="dropdown-content"
            role="listbox" .result="${this.result}"
            .selection="${this.selection}"
            .virtualFocusEnabled="${this.virtualFocusEnabled}"
            @selection-changed="${this.onSelectionChanged}"
            .selectedMatchIndex="${this.selectedMatchIndex}"
            @selected-match-index-changed="${this.onSelectedMatchIndexChanged}"
            @match-focusin="${this.onMatchFocusin}"
            @match-click="${this.onMatchClick}"
            @keyword-click="${this.onKeywordClick}"
            ?hidden="${!this.dropdownIsVisible}">
        </cr-searchbox-dropdown>
      </div>
      <div id="bottomControls">
        ${this.isFuseboxEnabled ? html`
        <div class="contextualEntrypointContainer
                    contextualEntrypointContainerCompact">
          <cr-composebox-file-inputs id="fileInputs" @file-change="${
      this.onFileChange_}">
            <div class="context-menu-container" id="contextMenuContainer">
              <cr-composebox-contextual-entrypoint-button id="context"
                  exportparts="context-menu-entrypoint-icon,
                               entrypoint-button"
                  class="upload-button"
                  .inputState="${this.inputState_}"
                  .energyEffectAnimationEnabled="${
                      this.energyEffectAnimationEnabled_}"
                  @context-menu-entrypoint-click="${
      this.onContextMenuEntrypointClick_}">
              </cr-composebox-contextual-entrypoint-button>
            </div>
          </cr-composebox-file-inputs>
        </div>
        ` : ''}
        <div id="actionButtons">
          ${
              this.showVoiceSearchButton_() ? html`
          <div class="searchbox-icon-button-container voice">
            <button id="voiceSearchButton" class="searchbox-icon-button"
                tabindex="${this.virtualFocusEnabled &&
                    this.dropdownIsVisible ? -1 : 0}"
                @click="${this.onVoiceSearchButtonClick_}"
                title="${this.i18n('voiceSearchButtonLabel')}">
            </button>
          </div>
          ` :
              ''}
          ${this.showLensSearchButton_() ? html`
          <div class="searchbox-icon-button-container lens ${
              this.isScreenshotMenuOpen ? 'menu-open' : ''}">
            <button id="lensSearchButton" class="searchbox-icon-button"
                tabindex="${this.virtualFocusEnabled &&
                    this.dropdownIsVisible ? -1 : 0}"
                @click="${this.onLensSearchClick_}"
                title="${this.i18n('lensSearchButtonLabel')}">
            </button>
          </div>
          ` :
              ''}
        </div>
      </div>
    </div>
  `;
}
