// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {NtpSinglePromoElement} from './ntp_single_promo.js';

export function getHtml(this: NtpSinglePromoElement) {
  return html`
  <div id="bodyIcon" role="image" aria-label="${this.bodyIconName_}">
    <cr-icon icon="ntp-promo:${this.bodyIconName_}"></cr-icon>
  </div>
  <p id="bodyText">
    ${this.bodyText_}
  </p>
  <cr-button id="actionButton" @click="${this.onButtonClick_}"
      role="button" aria-label="${this.actionButtonText_}">
    ${this.actionButtonText_}
  </cr-button>`;
}
