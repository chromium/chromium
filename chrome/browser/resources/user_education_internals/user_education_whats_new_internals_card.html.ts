// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {UserEducationWhatsNewInternalsCardElement} from './user_education_whats_new_internals_card.ts';

export function getHtml(this: UserEducationWhatsNewInternalsCardElement) {
  if (!(this.item && this.type)) {
    return '';
  }
  // clang-format off
  return html`
<div class="card-content">
  <h3>${this.item.displayTitle}</h3>
  <cr-expand-button ?expanded="${this.dataExpanded_}"
      @expanded-changed="${this.onDataExpandedChanged_}">
    <div id="label"><b>Stored Data</b></div>
  </cr-expand-button>
  <div id="data" ?hidden="${!this.dataExpanded_}">
    ${this.isModule_() ? html`
      <p><b>Module name: </b>${this.formatName_()}</p>
      <p><b>Has browser command: </b>${this.formatHasBrowserCommand_()}</p>
      <p><b>Is feature enabled: </b>${this.formatIsFeatureEnabled_()}</p>
      <p><b>Queue position: </b>${this.formatQueuePosition_()}</p>` :
      ''}
    ${this.isEdition_() ? html`
      <p><b>Edition name: </b>${this.formatName_()}</p>
      <p><b>Is feature enabled: </b>${this.formatIsFeatureEnabled_()}</p>
      <p><b>Has been used: </b>${this.formatHasBeenUsed_()}</p>
      <p ?hidden="${!this.hasBeenUsed_()}">
        <b>Version used: </b>${this.formatVersionUsed_()}</p>` :
      ''}
    <cr-button id='clear' @click='${this.clearData_}'>
      Clear Data
    </cr-button>
  </div>
</div>`;
  // clang-format on
}
