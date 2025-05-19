// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ProtectedAudienceMeasurement} from './protected_audience_measurement.js';

export function getHtml(this: ProtectedAudienceMeasurement) {
  return html`
    <div>Protected Audience Measurement Notice Placeholder</div>
    <cr-button id="ackButton" @click="${this.onAck}">
      Ack Placeholder
    </cr-button>
  `;
}
