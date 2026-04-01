// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getHtml as getDropdownHtml} from '//resources/cr_components/searchbox/searchbox_searchbox_dropdown.html.js';
import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {LensSearchboxElement} from './lens_searchbox.js';

export function getHtml(this: LensSearchboxElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="inputWrapper" @focusout="${this.onInputWrapperFocusout}"
    @keydown="${this.onInputWrapperKeydown}">
  <div id="inputInnerContainer">
    <cr-searchbox-icon id="icon" .match="${this.selectedMatch}"
        default-icon="${this.searchboxIcon_}" in-searchbox>
    </cr-searchbox-icon>
    ${this.showThumbnail ? html`
      <div id="thumbnailContainer">
        <cr-searchbox-thumbnail thumbnail-url_="${this.thumbnailUrl_}"
            ?is-deletable_="${this.isThumbnailDeletable_}"
            @remove-thumbnail-click="${this.onRemoveThumbnailClick_}"
            role="button" aria-label="${this.i18n('searchboxThumbnailLabel')}"
            tabindex="${this.getThumbnailTabindex_()}">
        </cr-searchbox-thumbnail>
      </div>
    ` : nothing}
    <cr-searchbox-input id="input"
        exportparts="searchbox-input"
        .dropdownIsVisible="${this.dropdownIsVisible}"
        .inputAriaLive="${this.inputAriaLive}"
        .multiLineEnabled="${this.multiLineEnabled}"
        .searchboxIcon="${this.searchboxIcon_}"
        .inputHasMatches="${this.hasMatches()}"
        .searchboxAriaDescription="${this.searchboxAriaDescription}"
        .selectedMatch="${this.selectedMatch}"
        .placeholderText="${this.computePlaceholderText_(this.placeholderText)}"
        @input-focus-changed="${this.onInputFocusChanged}">
    </cr-searchbox-input>
  </div>
  ${getDropdownHtml.bind(this as any)()}
  ${this.searchboxLensSearchEnabled_? html`
    <div class="searchbox-icon-button-container lens">
    <button id="lensSearchButton" class="searchbox-icon-button lens"
        @click="${this.onLensSearchClick_}"
        title="${this.i18n('lensSearchButtonLabel')}">
    </button>
    </div>
  ` : ''}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
