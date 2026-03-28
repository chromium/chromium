// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/searchbox/searchbox_input.js';

import type {SearchboxElement} from '//resources/cr_components/searchbox/searchbox.js';
import {getHtml as getContextualEntrypointHtml} from '//resources/cr_components/searchbox/searchbox_contextual_entrypoint.html.js';
import {getHtml as getDropdownHtml} from '//resources/cr_components/searchbox/searchbox_searchbox_dropdown.html.js';
import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {NtpSearchboxElement} from './ntp_searchbox.js';

export function getHtml(this: NtpSearchboxElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="inputWrapper" @focusout="${this.onInputWrapperFocusout}"
    @keydown="${this.onInputWrapperKeydown}"
    @dragenter="${this.dragAndDropHandler?.handleDragEnter}"
    @dragover="${this.dragAndDropHandler?.handleDragOver}"
    @dragleave="${this.dragAndDropHandler?.handleDragLeave}"
    @drop="${this.dragAndDropHandler?.handleDrop}">
  ${this.ntpRealboxNextEnabled ?
    html`
      <search-animated-glow animation-state="${this.animationState}" part="animated-glow">
      </search-animated-glow>
    ` : ''}
  <cr-searchbox-input id="input"
      exportparts="searchbox-input"
      ?dropdown-is-visible="${this.dropdownIsVisible}"
      input-aria-live="${this.inputAriaLive_}"
      ?multi-line-enabled="${this.multiLineEnabled}"
      placeholder-text="${this.computePlaceholderText_(this.placeholderText)}"
      searchbox-aria-description="${this.searchboxAriaDescription}"
      searchbox-icon="${this.searchboxIcon_}"
      .selectedMatch="${this.selectedMatch}"
      ?input-has-matches="${this.inputHasMatches_()}"
      ?allow-file-paste="${this.ntpRealboxNextEnabled}"
      @focusin="${this.onInputFocus_}"
      @searchbox-input-files-pasted="${this.onSearchboxInputFilesPasted_}"
      @searchbox-input-text-updated="${this.onInputTextUpdated_}"
      @searchbox-input-tab-or-mouse-clicked="${this.onInputFocusChanged}">
    ${this.ntpRealboxNextEnabled ? html`
      <div class="contextualEntrypointContainer contextualEntrypointContainerCompact" slot="contextual-entrypoint">
        ${getContextualEntrypointHtml.bind(this as SearchboxElement)()}
      </div>
    ` : ''}
    ${this.shouldShowVoiceLens_(this.searchboxVoiceSearchEnabled_) ? html`
      <div slot="action-buttons" class="searchbox-icon-button-container voice">
        <button id="voiceSearchButton" class="searchbox-icon-button"
            @click="${this.onVoiceSearchClick_}"
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
    ${getDropdownHtml.bind(this as SearchboxElement, this.ntpRealboxNextEnabled)()}
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
