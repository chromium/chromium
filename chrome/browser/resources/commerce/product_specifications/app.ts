// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';
import './product_selector.js';
import './table.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import type {BrowserProxy} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import {BrowserProxyImpl} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {Router} from './router.js';
import type {ProductInfo, ProductSpecificationsProduct} from './shopping_service.mojom-webui.js';
import type {TableColumn, TableElement, TableRow} from './table.js';

interface AggregatedProductData {
  info: ProductInfo;
  spec: ProductSpecificationsProduct|null;
}

function aggregateProductDataByClusterId(
    infos: ProductInfo[], specs: ProductSpecificationsProduct[]):
    Record<string, AggregatedProductData> {
  const aggregatedDatas: Record<string, AggregatedProductData> = {};
  infos.forEach((info, index) => {
    aggregatedDatas[info.clusterId.toString()] = {
      info,
      spec: specs[index]!,
    };
  });
  return aggregatedDatas;
}

export interface ProductSpecificationsElement {
  $: {summaryTable: TableElement};
}

export class ProductSpecificationsElement extends PolymerElement {
  static get is() {
    return 'product-specifications-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      showEmptyState_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      specsTable_: {
        type: Object,
        value: {},
      },
    };
  }

  private showEmptyState_: boolean;
  private specsTable_: {columns: TableColumn[], rows: TableRow[]};

  private shoppingApi_: BrowserProxy = BrowserProxyImpl.getInstance();

  constructor() {
    super();
    ColorChangeUpdater.forDocument().start();
  }

  override async connectedCallback() {
    super.connectedCallback();
    const router = Router.getInstance();
    const params = new URLSearchParams(router.getCurrentQuery());
    const urlsParam = params.get('urls');
    if (!urlsParam) {
      this.showEmptyState_ = true;
      return;
    }

    let urls: string[] = [];
    try {
      urls = JSON.parse(urlsParam);
    } catch (_) {
      return;
    }

    const {productSpecs} =
        await this.shoppingApi_.getProductSpecificationsForUrls(
            urls.map(url => ({url})));

    const rows: TableRow[] = [];
    productSpecs.productDimensionMap.forEach((value: string, key: bigint) => {
      rows.push({
        title: value,
        values: productSpecs.products.map(
            (p: ProductSpecificationsProduct) =>
                p.productDimensionValues.get(key)!.join(',')),
      });
    });

    const infos = await this.getInfoForUrls_(urls);
    const aggregatedDatas =
        aggregateProductDataByClusterId(infos, productSpecs.products);
    this.specsTable_ = {
      columns:
          Object.values(aggregatedDatas).map((data: AggregatedProductData) => {
            return {
              selectedItem: {
                title: data.spec ? data.spec.title : '',
                // TODO(b/335637140): Replace with actual URL once available.
                url: 'https://example.com',
                imageUrl: data.info.imageUrl.url,
              },
            };
          }),
      rows,
    };
  }

  private async getInfoForUrls_(urls: string[]): Promise<ProductInfo[]> {
    const infos: ProductInfo[] = [];
    for (const url of urls) {
      const {productInfo} = await this.shoppingApi_.getProductInfoForUrl({url});
      if (productInfo.clusterId) {
        infos.push(productInfo);
      }
    }
    return infos;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'product-specifications-app': ProductSpecificationsElement;
  }
}

customElements.define(
    ProductSpecificationsElement.is, ProductSpecificationsElement);
