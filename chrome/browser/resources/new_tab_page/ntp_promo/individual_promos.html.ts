// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {IndividualPromosElement} from './individual_promos.js';

export function getHtml(this: IndividualPromosElement) {
  return html`
<div id="promos">
  ${this.eligiblePromos_.map(item => html`
  <button id="promo" @click="${this.onClick_.bind(this, item.id)}"
    aria-label="${item.buttonText}"
  >
    <cr-icon id="bodyIcon" icon="ntp-promo:${item.iconName}"></cr-icon>
    <p id="bodyText" class=${this.eligiblePromos_.length > 1 ?
       'multiplePromos' : 'singlePromo'}
    >${item.bodyText}</p>
    <cr-icon id="actionIcon" icon="cr:chevron-right"></cr-icon>
  </button>
  `)}
</div>
`;
}
