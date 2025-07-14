// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {NtpSinglePromoElement} from './ntp_single_promo.js';

export function getHtml(this: NtpSinglePromoElement) {
  return html`
  <cr-icon id="bodyIcon" icon="ntp-promo:${this.bodyIconName_}"></cr-icon>
  <p id="bodyText">
    ${this.bodyText_}
  </p>
  <cr-icon-button id="actionButton" iron-icon="cr:chevron-right"
      aria-label="${this.actionButtonText_}" @click="${this.onButtonClick_}">
  </cr-icon-button>`;
}
