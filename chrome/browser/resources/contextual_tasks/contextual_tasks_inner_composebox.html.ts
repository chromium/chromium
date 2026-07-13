// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import {ToolMode} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';

import type {ContextualTasksInnerComposeboxElement} from './contextual_tasks_inner_composebox.js';
import {getHtml as getContextMenuHtml} from './contextual_tasks_inner_composebox_context_menu.html.js';

export function getHtml(this: ContextualTasksInnerComposeboxElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    <ntp-error-scrim id="errorScrim" part="error-scrim"></ntp-error-scrim>
    <div id="composebox" part="composebox"
        @keydown="${this.onKeydown}"
        @focusin="${this.onComposeboxFocusin_}"
        @focusout="${this.onComposeboxFocusout_}"
        @dragenter="${this.dragAndDropHandler_.handleDragEnter}"
        @dragover="${this.dragAndDropHandler_.handleDragOver}"
        @dragleave="${this.dragAndDropHandler_.handleDragLeave}"
        @drop="${this.dragAndDropHandler_.handleDrop}">
      <div id="inputContainer" part="input-container">
        <cr-composebox-input id="composeboxInput"
            .entrypointName="${'ContextualTasks'}"
            exportparts="text-container, icon-container, mirror, input, smart-compose, cancel, action-icon, cancel-icon"
            .disableCaretColorAnimation="${this.disableCaretColorAnimation}"
            .showDropdown="${this.showDropdown}"
            .inputPlaceholder="${this.inputPlaceholder}"
            .input="${this.input}"
            .smartComposeEnabled="${this.smartComposeEnabled}"
            .smartComposeInlineHint="${this.smartComposeInlineHint}"
            .submitEnabled="${this.submitEnabled}"
            .cancelButtonTitle="${this.computeCancelButtonTitle()}"
            @input-input="${this.onInputInput}"
            @input-focusin="${this.onInputFocusin}"
            @cancel-click="${this.onCancelClick}">
        </cr-composebox-input>
        <cr-composebox-file-inputs id="fileInputs">
          <cr-composebox-file-carousel id="carousel">
          </cr-composebox-file-carousel>
          <cr-composebox-dropdown id="matches" part="dropdown"
              exportparts="match-text-container"
              role="listbox"
              .result="${this.result}"
              .selectedMatchIndex="${this.selectedMatchIndex}"
              .maxSuggestions="${this.maxSuggestions}"
              .toolMode="${this.inputState?.activeTool || ToolMode.kUnspecified}"
              .lastQueriedInput="${this.lastQueriedInput}"
              ?hidden="${!this.showDropdown || !this.dropdownNeeded}"
              @selected-match-index-changed="${this.onSelectedMatchIndexChanged}"
              @match-focusin="${this.onMatchFocusin}"
              @match-click="${this.onMatchClick}">
          </cr-composebox-dropdown>
          ${this.contextMenuEnabled ? getContextMenuHtml.bind(this)() : ''}
        </cr-composebox-file-inputs>
      </div>
      <cr-composebox-submit
          exportparts="action-icon, submit, submit-icon, submit-overlay"
          ?disabled="${!this.canSubmitFilesAndInput}"
          .iconType="${this.submitButtonIconType}"
          .submitButtonTitle="${this.i18n('composeboxSubmitButtonTitle')}"
          @submit-click="${this.onSubmitClick}"
          @submit-focusin="${this.onSubmitFocusin}">
      </cr-composebox-submit>
    </div>
  <!--_html_template_end_-->`;
  // clang-format on
}
