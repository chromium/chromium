/* Copyright 2026 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {HistoryFilterChipsElement} from './history_filter_chips.js';

export function getHtml(this: HistoryFilterChipsElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    <div class="filter-chip-container">
      <cr-chip
          id="userVisitsChip"
          ?selected="${this.isUserSelected}"
          @click="${this.onUserVisitsClick_}">
        ${this.isUserSelected ? html`<cr-icon icon="cr:check"></cr-icon>` : ''}
        $i18n{sourceFilterChipUser}
      </cr-chip>
      <cr-chip
          id="actorVisitsChip"
          ?selected="${this.isActorSelected}"
          @click="${this.onActorVisitsClick_}">
        ${this.isActorSelected ? html`<cr-icon icon="cr:check"></cr-icon>` : ''}
        $i18n{sourceFilterChipActor}
      </cr-chip>
    </div>
<!--_html_template_end_-->`;
  // clang-format on
}
