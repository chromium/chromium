// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OrganizerListSectionHeaderElement} from './organizer_list_section_header.js';

export function getHtml(this: OrganizerListSectionHeaderElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-expand-button id="expandButton" no-hover ?expanded="${this.expanded}"
    ?disabled="${this.disabled}" @expanded-changed="${this.onExpandedChanged_}">
  <slot></slot>
</cr-expand-button>
<cr-icon-button id="menuButton" iron-icon="cr:more-vert"
    aria-label="$i18n{tabGroupMoreOptions}" @click="${this.onMenuButtonClick_}">
</cr-icon-button>
<cr-action-menu id="menu">
  <button id="showSomeButton" class="dropdown-item"
      @click="${this.onShowSomeClick_}">
    $i18n{showSome}
  </button>
  <button id="showAllButton" class="dropdown-item"
      @click="${this.onShowAllClick_}">
    $i18n{showAll}
  </button>
</cr-action-menu>
<!--_html_template_end_-->`;
  // clang-format on
}
