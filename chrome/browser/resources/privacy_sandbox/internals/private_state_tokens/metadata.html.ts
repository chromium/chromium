// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PrivateStateTokensMetadataElement} from './metadata.js';

export function getHtml(this: PrivateStateTokensMetadataElement) {
  // clang-format off
  return html`
  <div class="cr-centered-card-container">
    <div class="card">
      <div class="cr-row" id="backRow">
        <div class="cr-padded-text" id="backRowText">
          <cr-icon-button iron-icon="cr:arrow-back"
              @click="${this.onClick_}"
              id="backButton">
          </cr-icon-button>
        </div>
      </div>
      <div class="cr-row first">
        <div class="cr-padded-text">
          <div>Issuer Origin</div>
          <div class="cr-secondary-text">${this.issuerOrigin}</div>
        </div>
      </div>
      <div class="cr-row">
        <div class="cr-padded-text">
          <div>Expiration</div>
          <div class="cr-secondary-text">${this.expiration}</div>
        </div>
      </div>
      <div class="cr-row">
        <div class="cr-padded-text">
          <div>Purposes</div>
          <div class="cr-secondary-text">
            ${this.purposes.map(item => html`<li>${item}</li>`)}
          </div>
        </div>
      </div>
  </div>`;
  // clang-format on
}
