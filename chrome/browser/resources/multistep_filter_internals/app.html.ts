// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {MultistepFilterInternalsAppElement} from './app.js';

export function getHtml(this: MultistepFilterInternalsAppElement) {
  // clang-format off
  return html`
    <h1 class="page-title">Multistep Filter Internals</h1>
    <div id="controls">
      <cr-input id="filter-input" placeholder="Search logs..."
          aria-label="Search logs"
          .value="${this.filterText}"
          @input="${this.onFilterInput_}">
      </cr-input>
      <cr-button id="clear-btn" class="action-button"
          @click="${this.onClearClick_}">
        Clear Logs
      </cr-button>
    </div>
    <div class="log-line header-line">
      <span class="text-time">Time</span>
      <span class="text-nav">Navigation ID</span>
      <span class="text-domain">Domain</span>
      <span class="text-event">Event</span>
      <span class="text-details">Details</span>
    </div>
    <div id="log-list">
      ${this.getFilteredLogs_().map(item => html`
        <div class="log-line">
          <span class="text-time">[${item.formattedTime}]</span>
          <span class="text-nav">
            [${item.navigationId !== 0n ?
                item.navigationId.toString() :
                'no-nav'}]
          </span>
          <span class="text-domain">
            ${item.sourceEtldPlus1 || 'no-domain'}
          </span>
          <span class="text-event">${item.eventType}</span>
          <span class="text-details">${item.details}</span>
        </div>
      `)}
    </div>
  `;
  // clang-format on
}
