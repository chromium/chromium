// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import '../strings.m.js';

import type {BrowserProxy} from '//resources/cr_components/commerce/browser_proxy.js';
import {BrowserProxyImpl} from '//resources/cr_components/commerce/browser_proxy.js';
import type {PriceInsightsInfo} from '//resources/cr_components/commerce/shopping_service.mojom-webui.js';
import {PriceInsightsInfo_PriceBucket} from '//resources/cr_components/commerce/shopping_service.mojom-webui.js';
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
  private shoppingApi_: BrowserProxy = BrowserProxyImpl.getInstance();

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
