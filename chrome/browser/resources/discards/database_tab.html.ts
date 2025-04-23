// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {DatabaseTabElement} from './database_tab.js';

export function getHtml(this: DatabaseTabElement) {
  //clang-format off
  return html`<!--_html_template_start_-->
<div>
  <div>
    Database Rows:
    ${this.optionalIntegerToString_(Number(this.size_.numRows))}
  </div>
  <div>
    Database Size:
    ${this.kilobytesToString_(Number(this.size_.onDiskSizeKb))}
  </div>
</div>
<table>
  <thead>
    <tr>
      <th data-sort-key="origin" class="sort-column"
          @click="${this.onSortClick}">
        <div class="header-cell-container">
          Origin
        </div>
      </th>
      <th data-sort-key="dirty" @click="${this.onSortClick}">
        <div class="header-cell-container">
          Dirty
        </div>
      </th>
      <th data-sort-key="lastLoaded" @click="${this.onSortClick}">
        <div class="header-cell-container">
          Last Loaded
        </div>
      </th>
      <th>
        <div class="header-cell-container">
          <div>
            <div>Updates Favicon</div>
            <div>In Background</div>
          </div>
        </div>
      </th>
      <th>
        <div class="header-cell-container">
          <div>
            <div>Updates Title</div>
            <div>In Background</div>
          </div>
        </div>
      </th>
      <th>
        <div class="header-cell-container">
          <div>
            <div>Used Audio</div>
            <div>In Background</div>
          </div>
        </div>
      </th>
      <th data-sort-key="cpuUsage" @click="${this.onSortClick}">
        <div class="header-cell-container">
          <div>
            <div>Average</div>
            <div>CPU Usage</div>
          </div>
        </div>
      </th>
      <th data-sort-key="memoryUsage" @click="${this.onSortClick}">
        <div class="header-cell-container">
          <div>
            <div>Average Memory</div>
            <div>Footprint</div>
          </div>
        </div>
      </th>
      <th data-sort-key="loadDuration" @click="${this.onSortClick}">
        <div class="header-cell-container">
          <div>
            <div>Average Load</div>
            <div>Time</div>
        </div>
        </div>
      </th>
    </tr>
  </thead>
  <tbody>
    ${this.getSortedRows_().map(item => html`
      <tr>
        <td class="origin-cell">${item.origin}</td>
        <td class="dirty-cell">${this.boolToString_(item.isDirty)}</td>
        <td>${this.lastUseToString_(item.value!.lastLoaded)}</td>
        <td>
          ${this.featureToString_(item.value!.updatesFaviconInBackground)}
        </td>
        <td>
          ${this.featureToString_(item.value!.updatesTitleInBackground)}
        </td>
        <td>${this.featureToString_(item.value!.usesAudioInBackground)}</td>
        <td>${this.getLoadTimeEstimate_(item, 'avgCpuUsageUs')}</td>
        <td>${this.getLoadTimeEstimate_(item, 'avgFootprintKb')}</td>
        <td>${this.getLoadTimeEstimate_(item, 'avgLoadDurationUs')}</td>
      </tr>
    `)}
  </tbody>
</table>
<div class="add-origin-container">
  <cr-input id="addOriginInput" label="Add Origin"
      value="${this.newOrigin_}"
      @value-changed="${this.onNewOriginChanged_}"
      @keydown="${this.onOriginKeydown_}" placeholder="https://example.org"
      ?invalid="${!this.isEmptyOrValidOrigin_(this.newOrigin_)}"
      error-message="The origin must be a valid URL without a path."
      autofocus>
    <button slot="suffix" label="Add Origin"
        @click="${this.onAddOriginClick_}"
        ?disabled="${!this.isValidOrigin_(this.newOrigin_)}">
      <cr-icon icon="cr:check"></cr-icon>
    </button>
  </cr-input>
</div>
<!--_html_template_end_-->`;
  //clang-format on
}
