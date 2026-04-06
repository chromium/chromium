// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import './catalog_attributes_row.js';
import './history_graph.js';
import './insights_comment_row.js';
import './price_tracking_section.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import type {ProductInfo} from '//resources/cr_components/commerce/shared.mojom-webui.js';
import type {PriceInsightsInfo} from '//resources/cr_components/commerce/shopping_service.mojom-webui.js';
import type {ShoppingServiceBrowserProxy} from '//resources/cr_components/commerce/shopping_service_browser_proxy.js';
import {ShoppingServiceBrowserProxyImpl} from '//resources/cr_components/commerce/shopping_service_browser_proxy.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {PriceInsightsBrowserProxyImpl} from './price_insights_browser_proxy.js';

export interface ShoppingInsightsAppElement {
  $: {
    insightsContainer: HTMLElement,
  };
}

export class ShoppingInsightsAppElement extends CrLitElement {
  static get is() {
    return 'shopping-insights-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      productInfo: {type: Object},
      priceInsightsInfo: {type: Object},
      isProductTrackable_: {type: Boolean},
      isProductTracked_: {type: Boolean},
    };
  }

  accessor productInfo: ProductInfo|undefined = undefined;
  accessor priceInsightsInfo: PriceInsightsInfo|undefined = undefined;
  protected accessor isProductTrackable_: boolean = false;
  protected accessor isProductTracked_: boolean = false;
  private shoppingApi_: ShoppingServiceBrowserProxy =
      ShoppingServiceBrowserProxyImpl.getInstance();

  constructor() {
    super();
    ColorChangeUpdater.forDocument().start();
  }

  override connectedCallback() {
    super.connectedCallback();

    // Allow any deferred rendering to take place before calling
    // showSidePanelUi().
    setTimeout(() => {
      PriceInsightsBrowserProxyImpl.getInstance().showSidePanelUi();
    }, 0);
  }

  override firstUpdated() {
    this.fetchData_();
  }

  private async fetchData_() {
    const {productInfo} = await this.shoppingApi_.getProductInfoForCurrentUrl();
    this.productInfo = productInfo;

    const {priceInsightsInfo} =
        await this.shoppingApi_.getPriceInsightsInfoForCurrentUrl();
    this.priceInsightsInfo = priceInsightsInfo;

    const {eligible} = await this.shoppingApi_.isShoppingListEligible();
    const {tracked} =
        await this.shoppingApi_.getPriceTrackingStatusForCurrentUrl();
    this.isProductTracked_ = tracked;
    this.isProductTrackable_ =
        eligible && (priceInsightsInfo.clusterId !== BigInt(0));
  }

  protected getRangeDescription_(): string {
    if (!this.priceInsightsInfo) {
      return '';
    }
    const lowPrice: string = this.priceInsightsInfo.typicalLowPrice;
    const highPrice: string = this.priceInsightsInfo.typicalHighPrice;
    if (this.priceInsightsInfo.hasMultipleCatalogs) {
      return lowPrice === highPrice ?
          loadTimeData.getStringF('rangeMultipleOptionsOnePrice', lowPrice) :
          loadTimeData.getStringF('rangeMultipleOptions', lowPrice, highPrice);
    }

    return lowPrice === highPrice ?
        loadTimeData.getStringF('rangeSingleOptionOnePrice', lowPrice) :
        loadTimeData.getStringF('rangeSingleOption', lowPrice, highPrice);
  }

  protected getHistoryTitle_(): string {
    if (!this.priceInsightsInfo) {
      return '';
    }
    return loadTimeData.getString(
        this.priceInsightsInfo.hasMultipleCatalogs ?
            'historyTitleMultipleOptions' :
            'historyTitleSingleOption');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'shopping-insights-app': ShoppingInsightsAppElement;
  }
}

customElements.define(
    ShoppingInsightsAppElement.is, ShoppingInsightsAppElement);
