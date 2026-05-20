// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ToolMode} from '//resources/cr_components/composebox/composebox_query.mojom-webui.js';
import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {NtpComposeboxElement} from './ntp_composebox.js';

export function getHtml(this: NtpComposeboxElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    <div id="composebox" part="composebox" @keydown="${this.onKeydown}">
      <div id="inputContainer" part="input-container">
        <cr-composebox-input id="composeboxInput"
            exportparts="text-container, icon-container, mirror, input, smart-compose, cancel, action-icon, cancel-icon"
            .showDropdown="${this.showDropdown}"
            .inputPlaceholder="${this.inputPlaceholder}"
            .input="${this.input}"
            .smartComposeEnabled="${this.smartComposeEnabled}"
            .smartComposeInlineHint="${this.smartComposeInlineHint}"
            .submitEnabled="${this.submitEnabled}"
            .cancelButtonTitle="${this.computeCancelButtonTitle_()}"
            @input-input="${this.onInputInput}"
            @input-focusin="${this.onInputFocusin}"
            @cancel-click="${this.onCancelClick}">
        </cr-composebox-input>
        <div id="context" part="context-entrypoint">
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
        </div>
      </div>
    </div>
  <!--_html_template_end_-->`;
  // clang-format on
}
