// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AutoTabGroupsResultsActionsElement} from './auto_tab_groups_results_actions.js';

export function getHtml(this: AutoTabGroupsResultsActionsElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="button-row">
  ${this.showClear ? html`
    <cr-button id="clearButton" class="tonal-button"
        aria-label="$i18n{clearAriaLabel}"
        @click="${this.onClearClick_}">
      $i18n{clearSuggestions}
    </cr-button>
  ` : ''}
  <cr-button id="createButton" class="action-button"
      @click="${this.onCreateGroupClick_}">
    ${this.getCreateButtonText_()}
  </cr-button>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
