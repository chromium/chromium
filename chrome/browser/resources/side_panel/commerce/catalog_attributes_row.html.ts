// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CatalogAttributesRowElement} from './catalog_attributes_row.js';

export function getHtml(this: CatalogAttributesRowElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${this.priceInsightsInfo ? html`
<div id="attributesRow">
  <div ?hidden="${!this.priceInsightsInfo.hasMultipleCatalogs}"
      class="attributes">
    ${this.priceInsightsInfo.catalogAttributes}
  </div>
  <a href="#" ?hidden="${!this.priceInsightsInfo.jackpot.length}"
      class="link" @click="${this.onJackpotClick_}">
    <span>$i18n{buyOptions}</span>
    <cr-icon icon="cr:open-in-new"></cr-icon>
  </a>
</div>` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
