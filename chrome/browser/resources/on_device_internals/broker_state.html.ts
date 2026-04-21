// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OnDeviceInternalsBrokerStateElement} from './broker_state.js';

export function getHtml(this: OnDeviceInternalsBrokerStateElement) {
  return html`
    <div class="card">
      <h2>Broker Properties</h2>
      <table>
        ${this.state_.properties.map(property => html`
          <tr>
            <td class="property-name">${property.description}</td>
            <td>${property.value}</td>
          </tr>
        `)}
      </table>
    </div>

    <div class="card">
      <h2>Assets</h2>
      <table>
        <thead>
          <tr>
            <th>Name</th>
            <th>Version</th>
            <th>State</th>
            <th>Error</th>
          </tr>
        </thead>
        <tbody>
          ${this.state_.assets.map(asset => html`
            <tr>
              <td>${asset.name}</td>
              <td>${asset.version || 'N/A'}</td>
              <td>${this.assetStateToString(asset.state)}</td>
              <td>${asset.error || 'None'}</td>
            </tr>
          `)}
        </tbody>
      </table>
    </div>

    <div class="card">
      <h2>Use Cases</h2>
      <table>
        <thead>
          <tr>
            <th>Name</th>
            <th>Requested</th>
            <th>Unavailable Reason</th>
          </tr>
        </thead>
        <tbody>
          ${
      this.state_.useCases.map(
          useCase => html`
            <tr>
              <td>${useCase.name}</td>
              <td>
                <input type="checkbox"
                    .checked="${useCase.assetsRequested}"
                    data-use-case="${useCase.name}"
                    @change="${this.onUseCaseRequestedChange_}">
              </td>
              <td>${
              this.unavailableReasonToString(useCase.unavailableReason)}</td>
            </tr>
          `)}
        </tbody>
      </table>
    </div>

    <div class="card">
      <h2>Models</h2>
      <table>
        <thead>
          <tr>
            <th>Name</th>
            <th>Weights Path</th>
          </tr>
        </thead>
        <tbody>
          ${this.state_.models.map(model => html`
            <tr>
              <td>${model.name}</td>
              <td class="path">${model.weightsPath}</td>
            </tr>
          `)}
        </tbody>
      </table>
    </div>

    <div class="card">
      <cr-button @click="${this.onUninstallModelsClick_}">
        Uninstall Models
      </cr-button>
    </div>
  `;
}
