// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ThreeAdsApisNotice} from './three_ads_apis_notice.js';

export function getHtml(this: ThreeAdsApisNotice) {
  return html`
    <div>Three Ads Apis Notice Placeholder</div>
    <cr-button id="ackButton" @click="${this.onAck}">
      Ack Placeholder
    </cr-button>
  `;
}
