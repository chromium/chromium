// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {LensSearchboxElement} from './lens_searchbox.js';

export function getHtml(this: LensSearchboxElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="inputWrapper" @focusout="${this.onInputWrapperFocusout}"
    @keydown="${this.onInputWrapperKeydown}">
  <cr-searchbox-input id="input"
      exportparts="searchbox-input"
      .inputAriaLive="${this.inputAriaLive}"
      .searchboxIcon="${this.searchboxIcon_}"
      .searchboxAriaDescription="${this.searchboxAriaDescription}"
      .selectedMatch="${this.selectedMatch}"
      .placeholderText="${this.computePlaceholderText_(this.placeholderText)}"
      ?input-has-matches="${this.hasMatches()}"
      ?multi-line-enabled="${this.multiLineEnabled}"
      ?dropdown-is-visible="${this.dropdownIsVisible}"
      @focusin="${this.onFocusin}"
      @searchbox-input-text-updated="${this.onSearchboxInputTextUpdated}"
      @input-focus-changed="${this.onInputFocusChanged}">
    ${this.showThumbnail ? html`
      <div id="thumbnailContainer" slot="thumbnail">
        <cr-searchbox-thumbnail
            thumbnail-url="${this.thumbnailUrl_}"
            ?is-deletable="${this.isThumbnailDeletable_}"
            @remove-thumbnail-click="${this.onRemoveThumbnailClick_}"
            role="button" aria-label="${this.i18n('searchboxThumbnailLabel')}"
            tabindex="${this.getThumbnailTabindex_()}">
        </cr-searchbox-thumbnail>
      </div>
    ` : nothing}
    ${this.searchboxLensSearchEnabled_? html`
      <div class="searchbox-icon-button-container lens">
        <button id="lensSearchButton" class="searchbox-icon-button lens"
            @click="${this.onLensSearchClick_}"
            title="${this.i18n('lensSearchButtonLabel')}">
        </button>
      </div>
    ` : ''}
  </cr-searchbox-input>
  <cr-searchbox-dropdown id="matches" part="searchbox-dropdown"
      exportparts="dropdown-content"
      role="listbox" .result="${this.result}"
      selected-match-index="${this.selectedMatchIndex}"
      @selected-match-index-changed="${this.onSelectedMatchIndexChanged}"
      @match-focusin="${this.onMatchFocusin}"
      @match-click="${this.onMatchClick}"
      ?hidden="${!this.dropdownIsVisible}">
  </cr-searchbox-dropdown>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
