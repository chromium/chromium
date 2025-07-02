// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AutofillMlInternalsAppElement} from './app.js';
import type {MLPredictionLog} from './autofill_ml_internals.mojom-webui.js';

export function getHtml(this: AutofillMlInternalsAppElement) {
  return html`
    <h1>Autofill ML Internals</h1>
    <h2>Prediction Logs</h2>
    <div id="logs-container">
      ${this.logs_.map((log: MLPredictionLog) => html`
        <div class="log-entry">
          <div>Form Signature: ${log.formSignature}</div>
        </div>
      `)}
    </div>
  `;
}
