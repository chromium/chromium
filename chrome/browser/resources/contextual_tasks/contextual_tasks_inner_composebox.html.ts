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
    <search-animated-glow id="animatedSearchElement"
        animation-state="${this.animationState}"
        entrypoint-name="ContextualTasks"
        .energyEffectAnimationEnabled="${this.energyEffectAnimationEnabled}"
        .isZeroState="${this.isZeroState}"
        .darkThemeColorsEnabled="${true}"
        exportparts="composebox-background">
    </search-animated-glow>
    ${this.errorMessage ?
        html`<ntp-error-scrim id="errorScrim" part="error-scrim"
            .errorMessage="${this.errorMessage}"
            @dismiss-error-scrim="${this.onDismissErrorScrim}">
        </ntp-error-scrim>`
    : ''}
    <div id="composebox" part="composebox" ?inert="${!!this.errorMessage}"
        @keydown="${this.onKeydown}"
        @focusin="${this.onComposeboxFocusin_}"
        @focusout="${this.onComposeboxFocusout_}"
        @dragenter="${this.dragAndDropHandler.handleDragEnter}"
        @dragover="${this.dragAndDropHandler.handleDragOver}"
        @dragleave="${this.dragAndDropHandler.handleDragLeave}"
        @drop="${this.dragAndDropHandler.handleDrop}"
        @paste="${this.onPaste}">
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
        <cr-composebox-file-inputs id="fileInputs"
            @file-change="${this.onFileChange}"
            .disableFileInputs="${this.shouldDisableFileInputs()}">
          <div id="carouselContainer" part="carousel-container">
            <div class="carousel-container-inner">
              ${this.showFileCarousel ? html`
                <cr-composebox-file-carousel
                  part="cr-composebox-file-carousel"
                  exportparts="thumbnail, thumbnail-title"
                  id="carousel"
                  class="${this.carouselOnTop_ ? 'top' : ''}"
                  .files="${this.getFilteredCarouselFiles()}"
                  ?enable-scrolling="${this.enableCarouselScrolling}"
                  @delete-file="${this.onDeleteFile}">
                </cr-composebox-file-carousel> ` : ''}
            </div>
          </div>
          ${this.shouldShowDivider() ? html`
          <div class="carousel-divider" part="carousel-divider"></div>
          ` : ''}
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
      ${this.showLensButton ? html`<cr-icon-button
          class="action-icon"
          id="lensIcon"
          part="action-icon lens-icon"
          title="${this.i18n('lensSearchButtonLabel')}"
          @click="${this.onLensClick_}"
          ?disabled="${this.lensButtonDisabled}"
          @mousedown="${this.onLensIconMousedown_}">
      </cr-icon-button>` : ''}
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
