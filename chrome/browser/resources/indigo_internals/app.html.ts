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
        <span class="status-label">Optimization Guide Status</span>
        <span class="status-value ${this.getOptimizationGuideStatusClass_()}">
          ${this.getOptimizationGuideStatusText_()}
        </span>
      </div>
      <div class="status-item">
        <span class="status-label">Combined Eligibility</span>
        <div class="status-value-group">
          <cr-button @click="${this.onFetchCombinedClick_}">Fetch</cr-button>
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

    <div class="card">
      <h2>Glic Integration</h2>
      <div class="status-item">
        <span class="status-label">Integration Status</span>
        <span class="status-value ${this.getIntegrationStatusClass_()}">
          ${this.getIntegrationStatusText_()}
        </span>
      </div>
      <div class="status-item">
        <span class="status-label">Active Prompt Key</span>
        <span class="monospace-value">
          ${this.currentPromptKey_ || 'N/A'}
        </span>
      </div>
      <div class="status-item prompt-item">
        <span class="status-label">Override Prompt</span>
        <span class="prompt-value">${this.overridePrompt_ || 'N/A'}</span>
      </div>
    </div>

    <div class="card">
      <h2>Loaded Prompts</h2>
      <table class="details-table prompts-table">
        <thead>
          <tr>
            <th class="key-cell">Key</th>
            <th>Prompt</th>
          </tr>
        </thead>
        <tbody>
          ${this.loadedPrompts_.length === 0 ? html`
            <tr>
              <td colspan="2" class="empty-cell">
                No prompts loaded. Verify that the prompts file exists.
              </td>
            </tr>
          ` : this.loadedPrompts_.map(item => html`
            <tr>
              <td class="key-cell">${item.key}</td>
              <td class="prompt-cell">${item.prompt}</td>
            </tr>
          `)}
        </tbody>
      </table>
    </div>
  </div>
<!--_html_template_end_-->`;
  // clang-format on
}
