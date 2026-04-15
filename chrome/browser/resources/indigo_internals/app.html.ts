// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {IndigoInternalsAppElement} from './app.js';

export function getHtml(this: IndigoInternalsAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
  <div class="header">
    <h1>Indigo Internals</h1>
  </div>

  <div class="content">
    <div class="card">
      <h2>Service Status</h2>
      <div class="status-item">
        <span class="status-label">Local Eligibility</span>
        <span class="status-value ${this.getLocalEligibilityClass_()}">
          ${this.getLocalEligibilityText_()}
        </span>
      </div>
      <div class="status-item">
        <span class="status-label">Combined Eligibility</span>
        <div class="status-value-group">
          <cr-button @click="${this.onFetchCombinedClick_}">Fetch</cr-button>
          <cr-button @click="${this.onInvalidateClick_}">Invalidate</cr-button>
        </div>
      </div>

      ${this.combinedEligibility_ ? html`
        <div class="details-section">
          <table class="details-table">
            <tr>
              <td>Can Generate Image</td>
              <td class="${this.getValueClass_(
                  this.combinedEligibility_.canGenerateImage)}">
                ${this.getValueText_(
                    this.combinedEligibility_.canGenerateImage)}
              </td>
            </tr>
            <tr>
              <td>Ready To Onboard</td>
              <td class="${this.getValueClass_(
                  this.combinedEligibility_.readyToOnboard)}">
                ${this.getValueText_(this.combinedEligibility_.readyToOnboard)}
              </td>
            </tr>
            <tr>
              <td>Has Onboarded Pref</td>
              <td class="${this.getValueClass_(
                  this.combinedEligibility_.hasOnboardedPref)}">
                ${this.getValueText_(
                    this.combinedEligibility_.hasOnboardedPref)}
              </td>
            </tr>
            <tr>
              <td>Remote: Supported for Account</td>
              <td class="${this.getValueClass_(
                  this.combinedEligibility_
                    .remoteEligibility
                    ?.isServiceSupportedForAccount)}">
                ${this.getValueText_(
                      this.combinedEligibility_
                        .remoteEligibility
                        ?.isServiceSupportedForAccount)}
              </td>
            </tr>
            <tr>
              <td>Remote: Has User Image</td>
              <td class="${this.getValueClass_(
                  this.combinedEligibility_.remoteEligibility?.hasUserImage)}">
                ${this.getValueText_(
                    this.combinedEligibility_.remoteEligibility?.hasUserImage)}
              </td>
            </tr>
          </table>
          <div class="time-stamp">Last updated: ${this.lastUpdated_}</div>
        </div>
      ` : ''}
    </div>
  </div>
<!--_html_template_end_-->`;
  // clang-format on
}
