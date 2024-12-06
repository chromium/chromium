// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OnDeviceInternalsModelStatusElement} from './model_status.js';

export function getHtml(this: OnDeviceInternalsModelStatusElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="cr-centered-card-container">
  <h3>Foundational Model</h3>
  <div class="card">
    <div class="cr-row first">
      <div class="cr-padded-text">Foundational model state:
      ${this.pageData_.modelState}</div>
    </div>
  </div>
  <h3>Foundational model criteria</h3>
  ${
    (Object.keys(this.pageData_.registrationCriteria).length === 0) ?
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
               ${
                 Object.keys(this.pageData_.registrationCriteria).map(key =>
                  html`
                    <tr>
                      <td>${key}</td>
                      <td>${this.pageData_.registrationCriteria[key]}</td>
                    </tr>
                  `)
               }
             </tbody>
           </table>
         </div>
       `
  }
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
        ${
          this.pageData_.suppModels.map(suppModel => html`
            <tr>
              <td>${suppModel.suppModelName}</td>
              <td>${suppModel.isReady ? 'Ready' : 'Not Ready'}</td>
            </tr>
          `)
        }
      </tbody>
    </table>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
