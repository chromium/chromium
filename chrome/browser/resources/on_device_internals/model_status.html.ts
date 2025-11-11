// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OnDeviceInternalsModelStatusElement} from './model_status.js';

export function getHtml(this: OnDeviceInternalsModelStatusElement) {
  const baseModel = this.pageData_.baseModel;
  const baseInfo = this.pageData_.baseModel.info;
  const criteria = this.pageData_.baseModel.registrationCriteria;
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="cr-centered-card-container">
  <h3>Foundational Model</h3>
  <div class="card">
    <div class="cr-row first">
      <div class="cr-padded-text">
        <div>
          Foundational model state:
          <span class="value">${baseModel.state}</span>
        </div>
      ${baseInfo ? html`
        <div>
          <div>
            Model Name:
            <span class="value">${baseInfo.name}</value>
          </div>
          <div>
            Version:
            <span class="value">${baseInfo.version}</value>
          </div>
          <div>
            Backend Type: <span class="value">${baseInfo.backendType}</value>
          </div>
          <div>
            File path:
            <span class="value">${baseInfo.filePath}</value>
          </div>
          <div>
            Folder size:
            <span class="value">
              ${(Number(baseInfo.fileSize) / 1024 / 1024).
                toLocaleString('en-US', {maximumFractionDigits : 2})} MiB
            </value>
          </div>
        </div>` : html``}
      </div>
    </div>
    <div class="cr-row">
      <div class="cr-padded-text">
        Model crash count (current/maximum):
        ${this.pageData_.modelCrashCount}/${this.pageData_.maxModelCrashCount}
      </div>
      <cr-button class="cr-button-gap"
          @click="${this.onResetModelCrashCountClick_}">Reset</cr-button>
      <span id="needs-restart" class="cr-button-gap"
          ?hidden="${!this.mayRestartBrowser_}">
        You may need to restart the browser for the changes to take effect.
      </span>
    </div>
  </div>
  <h3>Foundational model criteria</h3>
  ${(Object.keys(criteria).length === 0) ?
    html`
      <div class="card">
        <div class="cr-row first">
          <div class="cr-padded-text">
            Foundation model criteria is not available yet. Please refresh the
            page.
          </div>
        </div>
      </div>` :
    html`
      <div>
        <table id="criteria-table">
          <thead>
            <tr>
              <th>Property</th>
              <th>Value</th>
            </tr>
          </thead>
          <tbody>
            ${Object.keys(criteria).map(key => html`
              <tr>
                <td>${key}</td>
                <td>${criteria[key]}</td>
              </tr>`)}
            <tr>
              <td>Detected VRAM (MiB)</td>
              <td>${this.pageData_.performanceInfo.vramMb === 0n ?
                'Not Available' :
                this.pageData_.performanceInfo.vramMb}</td>
            </tr>
            <tr>
              <td>Minimum VRAM required (MiB)</td>
              <td>${this.pageData_.minVramMb}</td>
            </tr>
          </tbody>
        </table>
      </div>`}
  <h3>Feature Adaptations</h3>
  <div>
    <table id="feature-adaptations-table">
      <thead>
        <tr>
          <th>Name</th>
          <th>Version</th>
          <th>Recently Used</th>
          <th>Actions</th>
        </tr>
      </thead>
      <tbody>
        ${this.pageData_.featureAdaptations.map(adaptation => html`
          <tr>
            <td>${adaptation.featureName}</td>
            <td>${adaptation.version}</td>
            <td>${adaptation.isRecentlyUsed}</td>
            <td>
              <button @click="${() =>
                this.onFeatureUsageSetterClick_(adaptation.featureKey, true)
              }">set to true</button>
              <button @click="${() =>
                this.onFeatureUsageSetterClick_(adaptation.featureKey, false)
              }">set to false</button>
            </td>
          </tr>`)}
      </tbody>
    </table>
  </div>
  <h3>Supplementary Models</h3>
  <div>
    <table id="supp-models-table">
      <thead>
        <tr>
          <th>OPTIMIZATION_TARGET</th>
          <th>Status</th>
        </tr>
      </thead>
      <tbody>
        ${this.pageData_.suppModels.map(suppModel => html`
          <tr>
            <td>${suppModel.suppModelName}</td>
            <td>${suppModel.isReady ? 'Ready' : 'Not Ready'}</td>
          </tr>`)}
      </tbody>
    </table>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
