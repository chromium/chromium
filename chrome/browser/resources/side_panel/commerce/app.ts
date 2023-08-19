// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/mwb_element_shared_style.css.js';
import '../strings.m.js';
import './price_tracking_section.js';
import './history_graph.js';
import './catalog_attributes_row.js';
import './insights_comment_row.js';

import {ShoppingListApiProxy, ShoppingListApiProxyImpl} from '//shopping-insights-side-panel.top-chrome/shared/commerce/shopping_list_api_proxy.js';
import {PriceInsightsInfo, ProductInfo} from '//shopping-insights-side-panel.top-chrome/shared/shopping_list.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {listenOnce} from 'chrome://resources/js/util_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';

export interface ShoppingInsightsAppElement {
  $: {
    insightsContainer: HTMLElement,
  };
}

export class ShoppingInsightsAppElement extends PolymerElement {
  static get is() {
    return 'shopping-insights-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      productInfo: Object,
      priceInsightsInfo: Object,
      isProductTrackable_: {
        type: Boolean,
        value: false,
      },
    };
  }

  productInfo: ProductInfo;
  priceInsightsInfo: PriceInsightsInfo;
  private isProductTrackable_: boolean;
  private shoppingApi_: ShoppingListApiProxy =
      ShoppingListApiProxyImpl.getInstance();

  override async connectedCallback() {
    super.connectedCallback();

    // Push showInsightsSidePanelUI() callback to the event queue to allow
    // deferred rendering to take place.
    listenOnce(this.$.insightsContainer, 'dom-change', () => {
      setTimeout(() => this.shoppingApi_.showInsightsSidePanelUi(), 0);
    });

    const {productInfo} = await this.shoppingApi_.getProductInfoForCurrentUrl();
    this.productInfo = productInfo;

    const {priceInsightsInfo} =
        await this.shoppingApi_.getPriceInsightsInfoForCurrentUrl();
    this.priceInsightsInfo = priceInsightsInfo;

    const {eligible} = await this.shoppingApi_.isShoppingListEligible();
    this.isProductTrackable_ =
        eligible && (priceInsightsInfo.clusterId !== BigInt(0));
  }

  private getRangeDescription_(info: PriceInsightsInfo): string {
    const lowPrice: string = info.typicalLowPrice;
    const highPrice: string = info.typicalHighPrice;
    if (info.hasMultipleCatalogs) {
      return lowPrice === highPrice ?
          loadTimeData.getStringF('rangeMultipleOptionsOnePrice', lowPrice) :
          loadTimeData.getStringF('rangeMultipleOptions', lowPrice, highPrice);
    }

    return lowPrice === highPrice ?
        loadTimeData.getStringF('rangeSingleOptionOnePrice', lowPrice) :
        loadTimeData.getStringF('rangeSingleOption', lowPrice, highPrice);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'shopping-insights-app': ShoppingInsightsAppElement;
  }
}

customElements.define(
    ShoppingInsightsAppElement.is, ShoppingInsightsAppElement);
