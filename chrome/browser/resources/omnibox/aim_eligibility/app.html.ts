// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AimEligibilityAppElement} from './app.js';

export function getHtml(this: AimEligibilityAppElement) {
  // clang-format off
  return html`
    <div class="content">
      <div class="check-item ${
          this.getCheckClass_(this.eligibilityState_.isEligible)}">
        <span class="check-value ${
            this.getCheckClass_(this.eligibilityState_.isEligible)}">
          ${this.getEligibilityText_()}
        </span>
      </div>
      <div class="check-label">AIModeSettings Policy:</div>
      <div class="check-item">
        <span class="check-value ${
            this.getCheckClass_(
                this.eligibilityState_.isEligibleByPolicy)}">
          ${this.getPolicyEligibilityText_()}
        </span>
      </div>
      <div class="check-label">Default Search Engine:</div>
      <div class="check-item">
        <span class="check-value ${
            this.getCheckClass_(this.eligibilityState_.isEligibleByDse)}">
          ${this.getDseEligibilityText_()}
        </span>
      </div>
      ${this.eligibilityState_.isServerEligibilityEnabled ? html`
        <div class="check-label">Server Eligibility:</div>
        <div class="check-item">
          <span class="check-value ${
              this.getCheckClass_(
                  this.eligibilityState_.isEligibleByServer)}">
            ${this.getServerEligibilityText_()}
          </span>
        </div>
        <div class="check-label">Eligibility Response Source:</div>
        <div class="check-item">
          <span class="check-value">
            ${this.eligibilityState_.serverResponseSource}
          </span>
        </div>
        <div class="check-label">Eligibility Response:</div>
        <div class="check-item">
          <input class="response-input ${this.inputState_}"
              .value="${this.eligibilityState_.serverResponseBase64Encoded}"
              @input="${this.onResponseInput_}"
              placeholder="Base64 encoded server response">
          </input>
          <div class="response-button-row">
            <cr-button @click="${this.onServerRequestClick_}">
              Request
            </cr-button>
            <cr-button
                ?disabled="${!this.eligibilityState_.serverResponseBase64UrlEncoded}"
                @click="${this.onViewResponseClick_}">
              View
            </cr-button>
            <cr-button @click="${this.onDraftResponseClick_}">
              Draft
            </cr-button>
            <cr-button @click="${this.onSaveResponseClick_}">
              Save
            </cr-button>
          </div>
        </div>
      ` : ''}
    </div>
    <div class="footer">
      Last updated: ${this.getLastUpdatedTimestamp_()}
    </div>
  `;
  // clang-format on
}
