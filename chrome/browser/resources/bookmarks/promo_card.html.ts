// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PromoCardElement} from './promo_card.js';

export function getHtml(this: PromoCardElement) {
  return html`<!--_html_template_start_-->
<div id="promoCard" role="dialog">
  <img id="image" class="banner" alt="">
  <div id="promoContent">
    <h2 id="title" class="label">
      $i18n{bookmarkPromoCardTitle}
    </h2>
    <div id="description" class="cr-secondary-text label">
      ${this.batchUploadPromoData_.promoSubtitle}
    </div>
    <cr-button id="actionButton"
        class="action-button" @click="${this.onSaveToAccountClick_}">
      $i18n{saveToAccount}
    </cr-button>
  </div>
  <cr-icon-button id="closeButton" class="icon-clear no-overlap"
      @click="${this.onCloseClick_}" title="$i18n{close}">
  </cr-icon-button>
</div>
<!--_html_template_end_-->`;
}
