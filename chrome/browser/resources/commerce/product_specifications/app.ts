// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';
import './header.js';
import './new_column_selector.js';
import './product_selector.js';
import './table.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import type {BrowserProxy} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import {BrowserProxyImpl} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import type {PageCallbackRouter, ProductSpecificationsSet} from 'chrome://resources/cr_components/commerce/shopping_service.mojom-webui.js';
import type {Uuid} from 'chrome://resources/mojo/mojo/public/mojom/base/uuid.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import type {HeaderElement} from './header.js';
import type {ProductSelectorElement} from './product_selector.js';
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
  $: {
    header: HeaderElement,
    productSelector: ProductSelectorElement,
    summaryTable: TableElement,
  };
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
      setName_: String,

      showEmptyState_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      specsTable_: {
        type: Object,
        value: {
          columns: [],
          rows: [],
        },
      },
    };
  }

  private setName_: string;
  private showEmptyState_: boolean;
  private specsTable_: {columns: TableColumn[], rows: TableRow[]};

  private shoppingApi_: BrowserProxy = BrowserProxyImpl.getInstance();
  private callbackRouter_: PageCallbackRouter;
  private listenerIds_: number[] = [];

  constructor() {
    super();
    this.callbackRouter_ = this.shoppingApi_.getCallbackRouter();
    ColorChangeUpdater.forDocument().start();
  }

  override async connectedCallback() {
    super.connectedCallback();

    this.listenerIds_.push(
        this.callbackRouter_.onProductSpecificationsSetRemoved.addListener(
            (uuid: Uuid) => this.onSetRemoved_(uuid)),
        this.callbackRouter_.onProductSpecificationsSetUpdated.addListener(
            (set: ProductSpecificationsSet) => this.onSetUpdated_(set)));

    const router = Router.getInstance();
    const params = new URLSearchParams(router.getCurrentQuery());
    const idParam = params.get('id');
    if (idParam) {
      const {set} = await this.shoppingApi_.getProductSpecificationsSetByUuid(
          {value: idParam});
      if (set) {
        this.setName_ = set.name;
        this.populateTable_(set.urls.map(url => (url.url)));
        return;
      }
    }
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

    // TODO(b/338427523): Add UI for choosing the name.
    const setName = 'Product specs';
    this.setName_ = setName;
    const {createdSet} = await this.shoppingApi_.addProductSpecificationsSet(
        setName, urls.map(url => ({url})));
    if (createdSet) {
      window.history.replaceState(
          undefined, '', '?id=' + createdSet.uuid.value);
    }

    this.populateTable_(urls);
  }

  private async populateTable_(urls: string[]) {
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
    this.showEmptyState_ = this.specsTable_.columns.length === 0;
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds_.forEach(id => this.callbackRouter_.removeListener(id));
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

  private onUrlAdd_(e: CustomEvent<{url: string}>) {
    const urls = this.getTableUrls_();
    urls.push(e.detail.url);
    this.populateTable_(urls);
  }

  private onUrlChange_(e: CustomEvent<{url: string, index: number}>) {
    const urls = this.getTableUrls_();
    urls[e.detail.index] = e.detail.url;
    this.populateTable_(urls);
  }

  private getTableUrls_(): string[] {
    // Until b/335637140 is resolved, these will all be the same placeholder
    // URL and the table will not update as expected.
    return this.specsTable_.columns.map(
        (column: TableColumn) => column.selectedItem.url);
  }

  private onSetUpdated_(_: ProductSpecificationsSet) {
    // TODO(b:333378234): If the update is for the currently shown set, apply
    //                    the updates.
  }

  private onSetRemoved_(_: Uuid) {
    // TODO(b:333378234): If the UUID is for the shown set, clear the UI or
    //                    refresh to load the zero-state.
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'product-specifications-app': ProductSpecificationsElement;
  }
}

customElements.define(
    ProductSpecificationsElement.is, ProductSpecificationsElement);
