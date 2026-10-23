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
      ${this.showActorFilterDropDownVersion_() ? html`
        <div class="filter-chips">
          <cr-icon icon="cr:check" aria-hidden="true"></cr-icon>
          <select id="actorFilterChip" class="md-select"
              @change="${this.onActorSelectChange_}">
            <option value="all"
                .selected="${!this.isUserSelected() && !this.isActorSelected()}">
              $i18n{sourceFilterAllActors}
            </option>
            <option value="user" .selected="${this.isUserSelected()}">
              $i18n{sourceFilterChipUser}
            </option>
            <option value="actor" .selected="${this.isActorSelected()}">
              $i18n{sourceFilterChipActor}
            </option>
          </select>
        </div>
      ` : html`
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
      `}
    ` : ''}
    ${this.showDeviceFilter ? html`
      <div class="filter-chips">
        <cr-icon icon="cr:check" aria-hidden="true"></cr-icon>
        <select id="deviceFilterChip" class="md-select"
            @change="${this.onDeviceSelectChange_}">
          <option value="-1" .selected="${this.clientIds.length === 0}">
            $i18n{deviceFilterAllDevices}
          </option>
          ${this.devices.map((device, index) => html`
            <option value="${index}"
                .selected="${this.isDeviceSelected_(device)}">
              ${device.name}
            </option>
          `)}
        </select>
      </div>
    ` : ''}
    ${this.showTimePeriodFilter ? html`
      <div class="filter-chips">
        <cr-icon icon="cr:check" aria-hidden="true"></cr-icon>
        <!-- TODO(b/561604860): Handle time period selection. -->
        <select id="timePeriodSelect" class="md-select">
          ${this.timePeriodOptions.map(option => html`
            <option value="${option.id}">${option.label}</option>
          `)}
        </select>
      </div>
    ` : ''}
    ${this.hasActiveFilter_() ? html`
      <cr-button id="clearAllButton" @click="${this.onClearAllClick_}">
        $i18n{resetFilters}
      </cr-button>
    ` : ''}
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
