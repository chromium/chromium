// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.html.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../strings.m.js';

import {ShoppingListApiProxy, ShoppingListApiProxyImpl} from '//shopping-insights-side-panel.top-chrome/shared/commerce/shopping_list_api_proxy.js';
import {PriceInsightsInfo, PriceInsightsInfo_PriceBucket} from '//shopping-insights-side-panel.top-chrome/shared/shopping_list.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './catalog_attributes_row.html.js';

export class CatalogAttributesRow extends PolymerElement {
  static get is() {
    return 'catalog-attributes-row';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      priceInsightsInfo: Object,
    };
  }

  priceInsightsInfo: PriceInsightsInfo;
  private shoppingApi_: ShoppingListApiProxy =
      ShoppingListApiProxyImpl.getInstance();

  private openJackpot_() {
    this.shoppingApi_.openUrlInNewTab(this.priceInsightsInfo.jackpot);
    chrome.metricsPrivate.recordEnumerationValue(
        'Commerce.PriceInsights.BuyingOptionsClicked',
        this.priceInsightsInfo.bucket,
        PriceInsightsInfo_PriceBucket.MAX_VALUE + 1);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'catalog-attributes-row': CatalogAttributesRow;
  }
}

customElements.define(CatalogAttributesRow.is, CatalogAttributesRow);
