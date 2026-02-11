// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {GlicInternalsAppElement} from './glic_internals_app.js';

export function getHtml(this: GlicInternalsAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
  <div id="contents" class="tab-contents">
    <h2>Enablement State</h2>
    ${this.data_?.enablement ? html`
      <table>
        <tr>
          <th>Property</th>
          <th>Value</th>
        </tr>
        ${this.getTableData_().map(item => html`
          <tr>
            <td>${item.label}</td>
            <td class="status-${item.value}">
              ${item.value ? '✅' : '🚫'}
            </td>
          </tr>
        `)}
      </table>` :
      html`<h3 id="loadingMsg">Loading...</h3>`}
    <h2>Sub-features</h2>
    ${this.data_?.enablement ? html`
      <table>
        <tr>
          <th>Feature</th>
          <th>State</th>
        </tr>
        <tr>
          <td>Account is eligible for Live</td>
          <td class="status-${!this.data_.enablement.liveDisallowed}">
            ${!this.data_.enablement.liveDisallowed ? '✅' : '🚫'}
          </td>
        </tr>
        <tr>
          <td>Account is eligible for 'Create Image with Gemini'</td>
          <td class="status-${!this.data_.enablement.shareImageDisallowed}">
            ${!this.data_.enablement.shareImageDisallowed ? '✅' : '🚫'}
          </td>
        </tr>
        <tr>
          <td>Actuation eligibility</td>
          <td>
            ${this.getActuationEligibilityString_(
                this.data_.enablement.actuationEligibility)}
          </td>
        </tr>
      </table>` :
      html`<h3 id="loadingMsg">Loading...</h3>`}
    <h2>Configuration</h2>
    ${this.data_?.config ? html`
      <table>
        <tr>
          <th>Name</th>
          <th>Value</th>
        </tr>
        <tr>
          <td>Guest URL</td>
          <td>${this.data_.config.guestUrl}</td>
        </tr>
        <tr>
          <td>FRE guest URL</td>
          <td>${this.data_.config.freGuestUrl}</td>
        </tr>
      </table>` :
      html`<h3 id="loadingMsg">Loading...</h3>`}
    <h2>Guest URL Presets</h2>
    ${this.data_?.config ? html`
      <div class="presets-container">
        <label for="autopushInput">Autopush</label>
        <input
            id="autopushInput" .value="${this.data_.config.autopushGuestUrl}"
            @change="${this.onAutopushInputChange}">
        </input>
        <label for="stagingInput">Staging</label>
        <input
            id="stagingInput" .value="${this.data_.config.stagingGuestUrl}"
            @change="${this.onStagingInputChange}">
        </input>
        <label for="preprodInput">Preprod</label>
        <input
            id="preprodInput" .value="${this.data_.config.preprodGuestUrl}"
            @change="${this.onPreprodInputChange}">
        </input>
        <label for="prodInput">Prod</label>
        <input id="prodInput" .value="${this.data_.config.prodGuestUrl}"
            @change="${this.onProdInputChange}">
        </input>
        <div id="inputErrorMsg" class="hiddenElement">
            Invalid URL submitted: presets not updated
        </div>
        <cr-button @click="${this.onSavePresetsClick_}">Save</cr-button>
      </div>` :
      html`<h3 id="loadingMsg">Loading...</h3>`}
  </div>
<!--_html_template_end_-->`;
  // clang-format on
}
