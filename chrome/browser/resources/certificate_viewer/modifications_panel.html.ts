// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ModificationsPanelElement} from './modifications_panel.js';

export function getHtml(this: ModificationsPanelElement) {
  return html`<!--_html_template_start_-->
  <div class="vertical-box">
    <h3 role="heading">$i18n{trust}</h3>
    <div class="horizontal-row">
      <label for="trustStateSelect" class="modifications-row-label">
        $i18n{trustState}
      </label>
      <div class="input-with-error">
        <select id="trustStateSelect" class="md-select"
            .value="${this.trustStateValue}"
            @change="${this.onTrustStateChange_}"
            ?disabled="${!(this.isEditable && this.editControlsEnabled)}">
          <option value="0">$i18n{trustStateDistrusted}</option>
          <option value="1">$i18n{trustStateHint}</option>
          <option value="2">$i18n{trustStateTrusted}</option>
        </select>
        <div id="trustStateSelectError"
              ?hidden="${this.trustStateErrorMessage.length === 0}"
              class="error">
          ${this.trustStateErrorMessage}
        </div>
      </div>
    </div>
    <div class="horizontal-row" id="constraintListSection"
        ?hidden="${
      this.trustStateValue !== '2' || this.constraints.length === 0}">
      <div class="modifications-row-label">$i18n{constraints}</div>
      <div>
        ${this.constraints.map((constraint: string, index: number) => html`
            <div class="constraint">${constraint}
              <cr-icon-button id="constraint-delete-${index}"
                  data-index="${index}"
                  @click="${this.onDeleteConstraintClick_}"
                  ?hidden="${!this.isEditable}"
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
    <div class="horizontal-row" id="addConstraintSection"
        ?hidden="${!(this.isEditable && this.trustStateValue === '2')}">
      <div class="modifications-row-label">$i18n{addConstraints}</div>
      <cr-input
          id="addConstraintInput"
          aria-label="$i18n{addConstraints}"
          placeholder="$i18n{addConstraintsPlaceholder}"
          ?invalid="${this.addConstraintErrorMessage.length !== 0}"
          error-message="${this.addConstraintErrorMessage}"
          @keypress="${this.onConstraintKeyPress_}"
          value="">
        <cr-button id="addConstraintButton" slot="suffix"
            ?disabled="${!this.editControlsEnabled}"
            @click="${this.onAddConstraintClick_}">
          $i18n{add}
        </cr-button>
      </cr-input>
    </div>
  </div>
  <!--_html_template_end_-->`;
}
