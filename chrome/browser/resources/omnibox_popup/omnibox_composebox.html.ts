// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {hasAllowedInputs} from '//resources/cr_components/composebox/common.js';
import {ToolMode} from '//resources/cr_components/composebox/composebox_query.mojom-webui.js';
import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OmniboxComposeboxElement} from './omnibox_composebox.js';

export function getHtml(this: OmniboxComposeboxElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    <div id="composebox" part="composebox" ?inert="${!!this.errorMessage}"
      @keydown="${this.onKeydown}">
      <div id="inputContainer" part="input-container">
        <!-- TODO(crbug.com/486706573): Add back cancel button title and cancel click handler once added to mixin. -->
        <cr-composebox-input id="composeboxInput"
            exportparts="text-container, icon-container, mirror, input, smart-compose, cancel, action-icon, cancel-icon"
            .disableCaretColorAnimation="${this.disableCaretColorAnimation}"
            .showDropdown="${this.showDropdown}"
            .inputPlaceholder="${this.inputPlaceholder}"
            .input="${this.input}"
            .smartComposeInlineHint="${this.smartComposeInlineHint}"
            .submitEnabled="${this.submitEnabled}"
            .entrypointName="${this.entrypointName}"
            @input-input="${this.onInputInput}"
            @input-focusin="${this.onInputFocusin}">
        </cr-composebox-input>
        <div id="context" part="context-entrypoint">
          <!-- TODO(crbug.com/486706573): Add back shouldDisableFileInputs_ when added to mixin-->
          <cr-composebox-file-inputs id="fileInputs"
              @file-change="${this.onFileChange}">
                          ${this.shouldShowDivider() ? html`
            <div class="carousel-divider" part="carousel-divider"></div>
            ` : ''}
            <cr-composebox-dropdown
                id="matches"
                part="dropdown"
                exportparts="match-text-container"
                role="listbox"
                .result="${this.result}"
                .selectedMatchIndex="${this.selectedMatchIndex}"
                .maxSuggestions="${this.maxSuggestions}"
                .toolMode="${this.inputState?.activeTool || ToolMode.kUnspecified}"
                @selected-match-index-changed="${this.onSelectedMatchIndexChanged}"
                @match-focusin="${this.onMatchFocusin}"
                @match-click="${this.onMatchClick}"
                ?hidden="${!this.showDropdown || !this.dropdownNeeded}"
                .lastQueriedInput="${this.lastQueriedInput}">
            </cr-composebox-dropdown>
            ${this.contextMenuEnabled ? html`
              <div class="context-menu-container" id="contextMenuContainer"
                  part="context-menu-and-tools"
                  @mousedown="${this.onContextMenuContainerMousedown}"
                  @click="${this.onContextMenuContainerClick}">
                ${hasAllowedInputs(this.inputState, this.usePecApi) ? html`
                  <cr-composebox-contextual-entrypoint-button
                      id="contextEntrypoint"
                      part="composebox-entrypoint"
                      exportparts="context-menu-entrypoint-icon, entrypoint-button"
                      class="upload-button no-overlap"
                      .inputState="${this.inputState}"
                      ?upload-button-disabled="${this.uploadButtonDisabled}"
                      ?show-context-menu-description="${this.showContextMenuDescription}">
                  </cr-composebox-contextual-entrypoint-button>
                ` : ''}
                <!-- TODO(crbug.com/508287630): Add tool chips and carousel. -->
              </div>
            ` : ''}
          </cr-composebox-file-inputs>
        </div>
      </div>
    </div>
<!--_html_template_end_-->`;
  // clang-format on
}
