// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CardsElement} from './cards.js';

export function getHtml(this: CardsElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="showToggleContainer" @click="${this.onShowToggleClick_}">
  <div id="showToggleTitle">$i18n{showCardsToggleTitle}</div>
  <cr-policy-indicator indicator-type="devicePolicy"
      ?hidden="${!this.managedByPolicy_}">
  </cr-policy-indicator>
  <cr-toggle title="$i18n{showCardsToggleTitle}" ?checked="${this.show_}"
      ?disabled="${this.managedByPolicy_}"
      @change="${this.onShowChange_}">
  </cr-toggle>
</div>
<div id="cards">
  <cr-collapse ?opened="${this.show_}" ?no-animation="${!this.initialized_}">
    <hr class="sp-hr">
    ${this.modules_.map((item, index) => html`
      <div class="card" data-index="${index}" @click="${this.onCardClick_}">
        <div class="card-name">${item.name}</div>
        <cr-checkbox class="card-checkbox" data-index="${index}"
            ?checked="${item.enabled}" ?disabled="${this.managedByPolicy_}"
            title="${item.name}" @change="${this.onCardCheckboxChange_}">
        </cr-checkbox>
      </div>
      ${this.showCartOptionCheckbox_(item.id, item.enabled) ? html`
        <div class="card" id="cartCard" @click="${this.onCartCardClick_}">
          <div class="card-option-name"
              id="cartOption" >$i18n{modulesCartSentence}</div>
          <cr-checkbox class="card-checkbox"
              ?checked="${this.cartOptionCheckbox_}"
              ?disabled="${this.managedByPolicy_}"
              title="$i18n{modulesCartSentence}"
              @change="${this.onCartCheckboxChange_}">
          </cr-checkbox>
        </div>
      ` : ''}
      ${this.showDiscountOptionCheckbox_(item.id, item.enabled) ? html`
        <div class="card" id="discountCard"
            @click="${this.onDiscountCardClick_}">
          <div class="card-option-name"
              id="discountOption">$i18n{modulesCartDiscountConsentAccept}</div>
          <cr-checkbox class="card-checkbox"
              ?checked="${this.discountCheckbox_}"
              ?disabled="${this.managedByPolicy_}"
              title="$i18n{modulesCartDiscountConsentAccept}"
              @change="${this.onDiscountCheckboxChange_}">
          </cr-checkbox>
        </div>
      ` : ''}
    `)}
  </cr-collapse>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
