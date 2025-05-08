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
            File path:
            <span class="value">${baseInfo.filePath}</value>
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
           </tbody>
         </table>
       </div>`}
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
