// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '/strings.m.js';

import type {PriceInsightsInfo} from '//resources/cr_components/commerce/shopping_service.mojom-webui.js';
import {PriceInsightsInfo_PriceBucket} from '//resources/cr_components/commerce/shopping_service.mojom-webui.js';
import type {ShoppingServiceBrowserProxy} from '//resources/cr_components/commerce/shopping_service_browser_proxy.js';
import {ShoppingServiceBrowserProxyImpl} from '//resources/cr_components/commerce/shopping_service_browser_proxy.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './catalog_attributes_row.css.js';
import {getHtml} from './catalog_attributes_row.html.js';

export class CatalogAttributesRowElement extends CrLitElement {
  static get is() {
    return 'catalog-attributes-row';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      priceInsightsInfo: {type: Object},
    };
  }

  accessor priceInsightsInfo: PriceInsightsInfo = {
    clusterId: BigInt(0),
    typicalLowPrice: '',
    typicalHighPrice: '',
    catalogAttributes: '',
    jackpot: '',
    bucket: PriceInsightsInfo_PriceBucket.MIN_VALUE,
    hasMultipleCatalogs: false,
    history: [],
    locale: '',
    currencyCode: '',
  };
  private shoppingApi_: ShoppingServiceBrowserProxy =
      ShoppingServiceBrowserProxyImpl.getInstance();

  protected onJackpotClick_() {
    this.shoppingApi_.openUrlInNewTab(this.priceInsightsInfo.jackpot);
    chrome.metricsPrivate.recordEnumerationValue(
        'Commerce.PriceInsights.BuyingOptionsClicked',
        this.priceInsightsInfo.bucket,
        PriceInsightsInfo_PriceBucket.MAX_VALUE + 1);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'catalog-attributes-row': CatalogAttributesRowElement;
  }
}

customElements.define(
    CatalogAttributesRowElement.is, CatalogAttributesRowElement);
