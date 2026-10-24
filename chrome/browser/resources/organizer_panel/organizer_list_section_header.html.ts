// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OrganizerListSectionHeaderElement} from './organizer_list_section_header.js';

export function getHtml(this: OrganizerListSectionHeaderElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-expand-button id="expandButton" no-hover ?expanded="${this.expanded}"
    ?disabled="${this.disabled}" @expanded-changed="${this.onExpandedChanged_}"
    @keydown="${this.onExpandButtonKeydown_}" expand-title="$i18n{expand}"
    collapse-title="$i18n{collapse}">
  <slot></slot>
</cr-expand-button>
<cr-icon-button id="menuButton" iron-icon="cr:more-vert"
    aria-label="$i18n{options}" title="$i18n{options}"
    @click="${this.onMenuButtonClick_}" @keydown="${this.onMenuButtonKeydown_}">
</cr-icon-button>
<cr-action-menu id="menu" auto-close-on-focusout
    @open-changed="${this.onMenuOpenChanged_}">
  <button id="showSomeButton" class="dropdown-item" role="menuitemradio"
      aria-checked="${!this.showAll}" @click="${this.onShowSomeClick_}">
    <cr-icon id="showSomeCheck" icon="cr:check" aria-hidden="true"
        ?invisible="${this.showAll}">
    </cr-icon>
    $i18n{showSome}
  </button>
  <button id="showAllButton" class="dropdown-item" role="menuitemradio"
      aria-checked="${this.showAll}" @click="${this.onShowAllClick_}">
    <cr-icon id="showAllCheck" icon="cr:check" aria-hidden="true"
        ?invisible="${!this.showAll}">
    </cr-icon>
    $i18n{showAll}
  </button>
</cr-action-menu>
<!--_html_template_end_-->`;
  // clang-format on
}
