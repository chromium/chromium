// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {DisclosureAppElement} from './app.js';

export function getHtml(this: DisclosureAppElement) {
  return html`
  <div id="titleContainer">
    <div id="iconContainer">
      <cr-icon icon="product-specifications:table-chart-organize">
      </cr-icon>
    </div>
    <div id="title" role="heading">
      ${this.i18n('disclosureTitle')}
    </div>
  </div>

  <div id="summary">
    <div id="itemsHeader">${this.i18n('disclosureItemsHeader')}</div>
    <div id="itemsContainer">
      ${this.items_.map(item => html`
        <div class="item">
          <cr-icon class="item-icon"
              icon="product-specifications-disclosure:${item.icon}">
          </cr-icon>
          <div>${item.text}</div>
        </div>`)}
    </div>
  </div>
  <localized-link id="learnMoreLink"
    .localizedString="${this.i18nAdvanced('disclosureLearnMore')}"
    @link-clicked="${this.onLearnMoreClicked_}"
    link-url="${this.i18n('compareLearnMoreUrl')}">
  </localized-link>
  <div id="actionsContainer">
    <cr-button class="tonal-button" @click=${this.declineDisclosure_}>
      ${this.i18n('declineDisclosure')}
    </cr-button>
    <cr-button class="action-button" @click=${this.acceptDisclosure_}>
      ${this.i18n('acceptDisclosure')}
    </cr-button>
  </div>`;
}
