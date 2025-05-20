// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {MeasurementNotice} from './measurement_notice.js';

export function getHtml(this: MeasurementNotice) {
  return html`
    <div>Measurement Notice Placeholder</div>
    <cr-button id="ackButton" @click="${this.onAck}">
      Ack Placeholder
    </cr-button>
  `;
}
