// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContextualTasksInternalsAppElement} from './app.js';

export function getHtml(this: ContextualTasksInternalsAppElement) {
  // clang-format off
  return html`
<h1>Contextual Tasks Internals</h1>
<cr-tab-box>
  <div slot="tab">Model Selection</div>
  <div slot="panel">
    <div class="card">
      <h2>Model Configuration</h2>
      <div class="model-options">
        <div class="container">
          <span class="mode-label">Tab selection mode: </span>
          <select id="tabSelectionModeSelect" class="md-select"
              value="${this.tabSelectionMode_}"
              @change="${this.onTabSelectionModeChange_}">
            <option value="kEmbeddingsMatch">Embeddings Match</option>
            <option value="kMultiSignalScoring">Multi Signal Scoring</option>
            <option value="kStaticSignalsOnly">Static Signals Scoring</option>
          </select>
        </div>
        <div class="container">
          <span class="slider-percentage">Min model score: ${this.minModelScore_}</span>
          <cr-slider id="minModelScoreSlider" aria-label="Min model score"
              min="0" max="1.0" .value="${this.minModelScore_}"
              @cr-slider-value-changed="${this.onMinModelScoreCrSliderValueChanged_}">
          </cr-slider>
        </div>
      </div>
    </div>

    <div class="card">
      <h2>Query Testing</h2>
      <cr-textarea type="text" id="textInput" label=""
          placeholder="Type query here..." .value="${this.query_}"
          @value-changed="${this.onQueryValueChanged_}">
      </cr-textarea>
      <cr-button class="action-button" ?disabled="${this.isQueryPending_}"
          @click="${this.onSubmitClick_}">
        Submit Query
      </cr-button>
    </div>

    <div class="card">
      <h2>Relevant Tabs</h2>
      <ul>
      ${this.relevantTabs_.map(item => html`
        <li>
          <a href="${item.url}" target="_blank">${item.title}</a>
        </li>
      `)}
      </ul>
    </div>

    <div class="card">
      <h2>Event Logs</h2>
      <table>
        <thead>
          <tr>
            <th class="time">Time</th>
            <th class="source-location">Source Location</th>
            <th class="message">Log Message</th>
          </tr>
        </thead>
        <tbody>
          ${this.eventLogMessages_.map(item => html`
            <tr>
              <td class="time">${item.eventTime.toLocaleTimeString()}</td>
              <td class="source-location">
                <a href="${item.sourceLinkURL}">${item.sourceLinkText}</a>
              </td>
              <td class="message">${item.message}</td>
            </tr>`)}
        </tbody>
      </table>
    </div>
  </div>

  <div slot="tab">Debugging</div>
  <div slot="panel">
    <div class="card debug-section">
      <h2>Host Override Settings</h2>
      <div class="debug-row">
        <span>Forced Embedded Page Host: </span>
        <cr-input id="forcedHostInput" label=""
            placeholder="e.g. https://example.c.googlers.com"
          .value="${this.forcedHost_}"
            @value-changed="${this.onForcedHostValueChanged_}">
        </cr-input>
        <cr-button class="action-button" style="margin-top: 0;" @click="${this.onSetForcedHostClick_}">
          Set Host
        </cr-button>
        <cr-button class="action-button" style="margin-top: 0;" @click="${this.onResetForcedHostClick_}">
          Reset
        </cr-button>
      </div>
      <div class="debug-row" style="margin-top: 8px;">
        <span>
        Currently set host: </span>
        <span class="current-host-display">${this.currentHost_ || 'None'}
      </span>
      </div>
    </div>
  </div>
  <div slot="tab">Eligibility</div>
  <div slot="panel">
    <div class="card eligibility-section">
      <div class="eligibility-status ${this.eligibilityState_?.isEligible ? 'eligible' : 'ineligible'}">
         ${this.eligibilityState_?.isEligible ? 'Eligible' : 'Not Eligible'}
      </div>
      ${!this.eligibilityState_?.isEligible ? html`
        <div class="eligibility-reasons">
          <h3>Reasons for Ineligibility:</h3>
          <ul>
            ${!this.eligibilityState_?.isContextualTasksEnabled ? html`<li>Feature flag (kContextualTasks) is not enabled. <strong>Tip:</strong> Enable it in chrome://flags.</li>` : ''}
            ${!this.eligibilityState_?.isSignedIn ? html`<li>User is not signed in with valid credentials. <strong>Tip:</strong> Sign in to the browser.</li>` : ''}
            ${!this.eligibilityState_?.primaryAccountInCookieJar ? html`<li>Primary account is not in the cookie jar. <strong>Tip:</strong> Are you signed in to google.com? Are you using multiple accounts?</li>` : ''}
            ${!this.eligibilityState_?.isAimEligible ? html`<li>User is not eligible for AI mode. <strong>Tip:</strong> Are you in a region when AIM is not allowed?</li>` : ''}
            ${!this.eligibilityState_?.isCobrowseEligible ? html`<li>User is not eligible for Co-Browse. <strong>Tip:</strong> The AimEligibilityService is disabling Cobrowsing. Debug at <a href="chrome://omnibox/aim-eligibility" target="_blank">chrome://omnibox/aim-eligibility</a></li>` : ''}
            ${!this.eligibilityState_?.isAimAllowedByPolicy ? html`<li>AIM is not allowed by enterprise policy.</li>` : ''}
            ${!this.eligibilityState_?.isContextSharingEnabled ? html`<li>Page context sharing enterprise policy is disabled.</li>` : ''}
          </ul>
        </div>
      ` : ''}
    </div>
  </div>
</cr-tab-box>
`;
  // clang-format on
}
