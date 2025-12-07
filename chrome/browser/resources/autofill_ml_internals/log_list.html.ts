// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {LogListElement} from './log_list.js';

export function getHtml(this: LogListElement) {
  // clang-format off
  return html`
<div class="filter-container">
  <label id="autofill-filter" class="chip filter-chip">
    <input type="checkbox" ?checked="${!this.hideAutofill_}"
        @change="${this.onAutofillCheckboxChange_}">
    Autofill
  </label>
  <label id="password-filter" class="chip filter-chip">
    <input type="checkbox" ?checked="${!this.hidePasswordManager_}"
        @change="${this.onPasswordManagerCheckboxChange_}">
    PWM
  </label>
  <cr-button @click="${this.onDownloadClick_}">
    Download JSON
  </cr-button>
</div>

${this.filteredLogEntries_.map((item, index) => html`
  <div class="log-entry ${this.getSelectedCssClass_(item)}"
      data-index="${index}" @click="${this.onLogClick_}">
    <div class="log-entry-row">
      <div class="log-info">
        <span class="chip ${this.getChipClass_(item.optimizationTarget)}">
          ${this.getOptimizationTargetText_(item.optimizationTarget)}
        </span>
        <span>${this.getPluralizedFields_(item.fieldPredictions.length)}</span>
      </div>
      <div class="timestamp">${this.formatTime_(item.startTime)}</div>
    </div>
    <div class="log-entry-row log-url">${item.formUrl.url}</div>
  </div>
`)}`;
  // clang-format on
}
