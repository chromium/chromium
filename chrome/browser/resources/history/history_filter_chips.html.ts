/* Copyright 2026 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {HistoryFilterChipsElement} from './history_filter_chips.js';

export function getHtml(this: HistoryFilterChipsElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="wrapper" style="position: relative;">
  <div class="filter-chip-container" role="group"
      aria-label="$i18n{sourceFilterChipsAriaLabel}">
    ${this.showActorFilter ? html`
      <cr-chip id="userVisitsChip" ?selected="${this.isUserSelected()}"
          @click="${this.onUserVisitsClick_}">
        <cr-icon icon="${this.getUserVisitsIcon_()}"></cr-icon>
        $i18n{sourceFilterChipUser}
      </cr-chip>
      <cr-chip id="actorVisitsChip" ?selected="${this.isActorSelected()}"
          @click="${this.onActorVisitsClick_}">
        <cr-icon icon="${this.getActorVisitsIcon_()}"></cr-icon>
        $i18n{sourceFilterChipActor}
      </cr-chip>
    ` : ''}
    ${this.showDeviceFilter ? html`
      <select id="deviceSelect" class="md-select">
        <!-- TODO(b/558659016): Replace hardcoded string with a localized string. -->
        <option>All devices</option>
        ${this.devices.map(device => html`
          <option>${device.name}</option>
        `)}
      </select>
    ` : ''}
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
