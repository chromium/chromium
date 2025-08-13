// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {MLFieldPredictionLog} from './autofill_ml_internals.mojom-webui.js';
import type {LogDetailsElement} from './log_details.js';

export function getHtml(this: LogDetailsElement) {
  // clang-format off
  return html`
    <h2>Details</h2>
    <div class="row">
      <span class="label">Form Signature</span>
      <span>${this.log.formSignature}</span>
    </div>
    <div class="row">
      <span class="label">URL</span>
      <span>${this.log.formUrl.url}</span>
    </div>
    <div class="row">
      <span class="label">Optimization Target</span>
      <span>
        ${this.getOptimizationTargetText_(this.log.optimizationTarget)}
      </span>
    </div>
    <div class="row">
      <span class="label">Duration</span>
      <span>${this.log.duration.microseconds} μs</span>
    </div>
    <h3>Fields</h3>
    ${this.log.fieldPredictions.map((field: MLFieldPredictionLog) => html`
      <div class="field">
        <div class="field-properties">
          <div class="row">
            <span class="label">Label</span>
            <span>${field.label}</span>
          </div>
          <div class="row">
            <span class="label">Placeholder</span>
            <span>${field.placeholder}</span>
          </div>
          <div class="row">
            <span class="label">Name</span>
            <span>${field.name}</span>
          </div>
          <div class="row">
            <span class="label">ID</span>
            <span>${field.id}</span>
          </div>
          <div class="row">
            <span class="label">Autocomplete</span>
            <span>${field.autocomplete}</span>
          </div>
          <div class="row">
            <span class="label">Form Control Type</span>
            <span>${field.formControlType}</span>
          </div>
        </div>
        <div class="histogram">
          ${this.getTopPredictions_(field.probabilities).map(prediction => html`
            <div class="histogram-row">
              <div class="histogram-label" title="${prediction.type}">
                ${prediction.type}
              </div>
              <div class="histogram-bar-container">
                <div class="histogram-bar"
                    style="width: ${prediction.percentage}">
                </div>
              </div>
              <div class="histogram-value">${prediction.percentage}</div>
            </div>
          `)}
        </div>
      `)}
    </div>
  `;
  // clang-format on
}
