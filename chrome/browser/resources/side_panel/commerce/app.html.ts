// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ShoppingInsightsAppElement} from './app.js';

export function getHtml(this: ShoppingInsightsAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="insightsContainer">
  ${this.productInfo && this.priceInsightsInfo ? html`
    <div class="sp-cards-separator"></div>
    <div class="section sp-card" id="titleSection">
      <h1 class="panel-title">${this.productInfo.clusterTitle}</h1>
      <div id="priceRange" class="panel-subtitle"
          ?hidden="${!this.priceInsightsInfo.typicalLowPrice.length}">
        ${this.getRangeDescription_()}
      </div>
      <catalog-attributes-row class="panel-subtitle"
          .priceInsightsInfo="${this.priceInsightsInfo}"
          ?hidden="${!!this.priceInsightsInfo.typicalLowPrice.length}">
      </catalog-attributes-row>
      <insights-comment-row class="section-details"
          ?hidden="${!!this.priceInsightsInfo.history.length}">
      </insights-comment-row>
    </div>
    ${this.priceInsightsInfo.history.length ? html`
      <div class="sp-cards-separator"></div>
      <div class="section sp-card" id="historySection">
        <div class="section-title" id="historyTitle">
          ${this.getHistoryTitle_()}
        </div>
        <catalog-attributes-row class="history-subtitle"
            .priceInsightsInfo="${this.priceInsightsInfo}"
            ?hidden="${!this.priceInsightsInfo.typicalLowPrice.length}">
        </catalog-attributes-row>
        <shopping-insights-history-graph
            .data="${this.priceInsightsInfo.history}"
            .locale="${this.priceInsightsInfo.locale}"
            .currency="${this.priceInsightsInfo.currencyCode}">
        </shopping-insights-history-graph>
        <insights-comment-row class="section-details">
        </insights-comment-row>
      </div>
    ` : ''}
    ${this.isProductTrackable_ ? html`
      <div class="sp-cards-separator"></div>
      <price-tracking-section class="section sp-card"
          id="priceTrackingSection"
          .productInfo="${this.productInfo}"
          .priceInsightsInfo="${this.priceInsightsInfo}"
          .isProductTracked="${this.isProductTracked_}">
      </price-tracking-section>
    ` : ''}
  ` : ''}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
