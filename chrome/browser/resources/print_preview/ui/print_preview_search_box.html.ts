// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PrintPreviewSearchBoxElement} from './print_preview_search_box.js';

export function getHtml(this: PrintPreviewSearchBoxElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-input type="search" id="searchInput" class="search-box-input"
    @search="${this.onSearchTermSearch}" @input="${this.onSearchTermInput}"
    aria-label="${this.label}" placeholder="${this.label}"
    autofocus="${this.autofocus}" spellcheck="false">
  <div slot="inline-prefix" id="icon" class="cr-icon icon-search" alt=""></div>
  <cr-icon-button id="clearSearch" class="icon-cancel"
      ?hidden="${!this.hasSearchText}" slot="suffix"
      @click="${this.onClearClick_}" title="$i18n{clearSearch}">
  </cr-icon-button>
</cr-input>
<!--_html_template_end_-->`;
  // clang-format on
}
