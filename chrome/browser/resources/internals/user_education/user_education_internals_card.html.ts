// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {UserEducationInternalsCardElement} from './user_education_internals_card.ts';

export function getHtml(this: UserEducationInternalsCardElement) {
  if (!this.promo) {
    return '';
  }
  // clang-format off
  return html`
<div class="card-content">
  <h3>${this.promo.displayTitle}
    <span ?hidden="${!this.showMilestone_()}"
        class="subtitle">[M${this.promo.addedMilestone}]</span></h3>
  <p ?hidden="${!this.showDescription_()}">
    ${this.promo.displayDescription}
  </p>
  <p><b>Type:</b> ${this.promo.type}</p>
  <p><b>Platforms: </b>${this.formatPlatforms_()}</p>
  <p ?hidden="${!this.showRequiredFeatures_()}">
    <b>Required features: </b>${this.formatRequiredFeatures_()}
  </p>
  <cr-expand-button ?hidden="${!this.showInstructions_()}"
      ?expanded="${this.instructionsExpanded_}"
      @expanded-changed="${this.onInstructionsExpandedChanged_}">
    <div id="label"><b>Full Details</b></div>
  </cr-expand-button>
  <ol id="instructions" ?hidden="${!this.instructionsExpanded_}">
    ${this.promo.instructions.map(item => html`<li>${item}</li>`)}
    <li ?hidden="${!this.showFollowedBy_()}">
      Followed by
      <a href="#${this.getFollowedByAnchor_()}"
        @click="${this.scrollToFollowedBy_}">
        ${this.promo.followedByInternalName}
      </a>
    </li>
  </ol>
  <cr-expand-button ?hidden="${!this.showData_()}"
      ?expanded="${this.dataExpanded_}"
      @expanded-changed="${this.onDataExpandedChanged_}">
    <div id="label"><b>Stored Data</b></div>
  </cr-expand-button>
  <div id="data" ?hidden="${!this.dataExpanded_}">
    ${this.promo.data.map(item => html`
      <p><b>${item.name}</b> ${item.value}</p>`)}
    <cr-button id="clear" @click="${this.clearData_}">
      Clear Data
    </cr-button>
  </div>
</div>
<cr-button class="action-button" ?hidden="${!this.showAction}" id="launch"
    @click="${this.launchPromo_}">
  Launch
</cr-button>`;
  // clang-format on
}
