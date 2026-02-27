// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PrivateStateTokensListItemElement} from './list_item.js';

export function getHtml(this: PrivateStateTokensListItemElement) {
  // clang-format off
  return html`
${this.redemptions.length === 0 ? html`
  <div class="cr-row ${this.index === 0 ? 'first' : ''}" id="row-content">
    <div class="cr-padded-text">
      <span>${this.issuerOrigin}</span>
      <span id='tokenText'>${this.getNumTokensString_()}</span>
      <span></span>
    </div>
    <cr-icon-button iron-icon="cr:info-outline"
        @click="${this.onUpdateMetadataUrlParamsClick_}">
    </cr-icon-button>
  </div>
` : html`
  <cr-expand-button id="expandButton"
      class="cr-row ${this.index === 0 ? 'first' : ''}"
      ?expanded="${this.expanded}"
      @expanded-changed="${this.onExpandedChanged_}">
    <span>${this.issuerOrigin}</span>
    <span id='tokenText'>${this.getNumTokensString_()}</span>
    <span>${this.getRedemptionsString_()}</span>
    <cr-icon-button iron-icon="cr:info-outline"
        @click="${this.onUpdateMetadataUrlParamsClick_}">
    </cr-icon-button>
  </cr-expand-button>
  <cr-collapse id="expandedContent" ?opened="${this.expanded}">
    ${this.redemptions.map(redemption => html`
      <div class="cr-padded-text hr">
        <span>${redemption.origin}</span>
        <span>&nbsp;${redemption.formattedTimestamp}</span>
      </div>
    `)}
  </cr-collapse>
`}`;
  // clang-format on
}
