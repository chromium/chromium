// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ConstraintListElement} from './constraint_list.js';

export function getHtml(this: ConstraintListElement) {
  // TODO(crbug.com/40928765): finish localization.
  return html`<!--_html_template_start_-->
  <div class="horizontal-row">
    <div class="modifications-row-label">$i18n{constraints}</div>
    <div id="constraintList">
    ${this.constraints.map((constraint: string, index: number) => html`
            <div class="constraint">${constraint}
              <cr-icon-button id="constraint-delete-${index}"
                  data-index="${index}"
                  @click="${this.onDeleteConstraintClick_}"
                  ?disabled="${!this.editControlsEnabled}"
                  class="icon-picture-delete">
              </cr-icon-button>
            </div>`)}
      <div id="constraintDeleteError"
          ?hidden="${this.deleteConstraintErrorMessage.length === 0}"
          class="error">
        ${this.deleteConstraintErrorMessage}
      </div>
    </div>
  </div>
  <div class="horizontal-row">
    <div class="modifications-row-label">Add Constraint</div>
    <cr-input
        id="addConstraintInput"
        placeholder="DNS or CIDR constraint"
        ?invalid="${this.addConstraintErrorMessage.length !== 0}"
        error-message="${this.addConstraintErrorMessage}">
      <cr-button id="addConstraintButton" slot="suffix"
          ?disabled="${!this.editControlsEnabled}"
          @click="${this.onAddConstraintClick_}">
        Add
      </cr-button>
    </cr-input>
  </div>
  <!--_html_template_end_-->`;
}
