// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ProtectedAudienceMeasurementNotice} from './protected_audience_measurement_notice.js';

export function getHtml(this: ProtectedAudienceMeasurementNotice) {
  return html`
    <div>Protected Audience Measurement Notice Placeholder</div>
    <cr-button id="ackButton" @click="${this.onAck}">
      Ack Placeholder
    </cr-button>
  `;
}
