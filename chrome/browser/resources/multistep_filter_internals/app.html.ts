// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {MultistepFilterInternalsAppElement} from './app.js';

export function getHtml(this: MultistepFilterInternalsAppElement) {
  // clang-format off
  return html`
    <h1 class="page-title">Multistep Filter Internals</h1>
    <div id="debug-info">
      ${this.debugInfo ? html`
        <div class="info-section">
          <h2>Overall Status</h2>
          <div class="info-grid">
            <div class="info-label">Overall Eligible:</div>
            <div class="info-value highlight-value">${this.debugInfo.isEligible ? 'Yes' : 'No'}</div>
          </div>
        </div>
        <div class="info-section">
          <h2>Account</h2>
          <div class="info-grid">
            <div class="info-label">User Signed In:</div>
            <div class="info-value">${this.debugInfo.accountStatus.isSignedIn ? 'Yes' : 'No'}</div>

            <div class="info-label">Model Execution Allowed:</div>
            <div class="info-value">${this.debugInfo.accountStatus.canUseModelExecutionFeatures ? 'Yes' : 'No'}</div>
          </div>
        </div>
        <div class="info-section">
          <h2>Settings</h2>
          <div class="info-grid">
            <div class="info-label">Smart Suggestions:</div>
            <div class="info-value">${this.debugInfo.settingsStatus.contextualCueingOptInState}</div>

            <div class="info-label">Enterprise Policy:</div>
            <div class="info-value">${this.debugInfo.settingsStatus.chromeSuggestionsPolicyState}</div>
          </div>
        </div>
        <div class="info-section">
          <h2>Consent</h2>
          <div class="info-grid">
            <div class="info-label">MSBB Enabled:</div>
            <div class="info-value">${this.debugInfo.consentStatus.isMsbbEnabled ? 'Yes' : 'No'}</div>

            <div class="info-label">History Sync Enabled:</div>
            <div class="info-value">${this.debugInfo.consentStatus.isHistorySyncEnabled ? 'Yes' : 'No'}</div>
          </div>
        </div>
        <div class="info-section">
          <h2>Feature Flags</h2>
          <div class="info-grid">
            ${this.debugInfo.featureFlags?.map(flag => html`
              <div class="info-label">${flag.name}:</div>
              <div class="info-value">${flag.enabled ? 'Enabled' : 'Disabled'}</div>
            `)}
          </div>
        </div>
      ` : html`
        <div class="loading-message">Loading debug info...</div>
      `}
    </div>
    <div id="controls">
      <cr-input id="filter-input" placeholder="Search logs..."
          aria-label="Search logs"
          .value="${this.filterText}"
          @input="${this.onFilterInput_}">
      </cr-input>
      <cr-button id="clear-btn" class="action-button"
          @click="${this.onClearClick_}">
        Clear Logs
      </cr-button>
    </div>
    <div class="log-line header-line">
      <span class="text-time">Time</span>
      <span class="text-nav">Navigation ID</span>
      <span class="text-host">Host</span>
      <span class="text-event">Event</span>
      <span class="text-details">Details</span>
    </div>
    <div id="log-list">
      ${this.getFilteredLogs_().map(item => html`
        <div class="log-line">
          <span class="text-time">[${item.formattedTime}]</span>
          <span class="text-nav">
            [${item.navigationId !== 0n ?
                item.navigationId.toString() :
                'no-nav'}]
          </span>
          <span class="text-host">
            ${item.host || 'no-host'}
          </span>
          <span class="text-event">${item.eventType}</span>
          <span class="text-details">${item.details}</span>
        </div>
      `)}
    </div>
  `;
  // clang-format on
}
