// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';
import './header.js';
import './loading_state.js';
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

interface LoadingState {
  loading: boolean;
  urlCount: number;
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
    loading: HTMLElement,
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
      loadingState_: Object,
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

  private minLoadingAnimationMs_: number = 500;
  private loadingState_: LoadingState = {loading: false, urlCount: 0};
  private setName_: string;
  private showEmptyState_: boolean;
  private specsTable_: {columns: TableColumn[], rows: TableRow[]};

  private shoppingApi_: BrowserProxy = BrowserProxyImpl.getInstance();
  private callbackRouter_: PageCallbackRouter;
  private listenerIds_: number[] = [];
  private id_: Uuid|null = null;

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
      this.id_ = {value: idParam};
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

    const setName = 'Product specs';
    this.setName_ = setName;
    const {createdSet} = await this.shoppingApi_.addProductSpecificationsSet(
        setName, urls.map(url => ({url})));
    if (createdSet) {
      this.id_ = createdSet.uuid;
      window.history.replaceState(
          undefined, '', '?id=' + createdSet.uuid.value);
    }

    this.populateTable_(urls);
  }

  resetMinLoadingAnimationMsForTesting(newValue = 0) {
    this.minLoadingAnimationMs_ = newValue;
  }

  private async populateTable_(urls: string[]) {
    const start = Date.now();
    this.showEmptyState_ = false;
    this.loadingState_ = {loading: true, urlCount: urls.length};

    let aggregatedDatas: Record<string, AggregatedProductData> = {};
    const rows: TableRow[] = [];
    if (urls.length) {
      const {productSpecs} =
          await this.shoppingApi_.getProductSpecificationsForUrls(
              urls.map(url => ({url})));
      productSpecs.productDimensionMap.forEach((title: string, key: bigint) => {
        const descriptions: string[] = [];
        const summaries: string[] = [];
        productSpecs.products.forEach(
            (product: ProductSpecificationsProduct) => {
              const value = product.productDimensionValues.get(key);
              descriptions.push(
                  (value?.specificationDescriptions || [])
                      .flatMap(description => description.options)
                      .flatMap(option => option.descriptions)
                      .map(descText => descText.text)
                      .join(', ') ||
                  '');
              summaries.push(
                  (value?.summary || [])
                      .map(summary => summary?.text || '')
                      ?.join(' ') ||
                  '');
            });
        rows.push({title, descriptions, summaries});
      });
      const infos = await this.getInfoForUrls_(urls);
      aggregatedDatas =
          aggregateProductDataByClusterId(infos, productSpecs.products);
    }

    // Enforce a minimum amount of time in the loading state to avoid it
    // appearing like an unintentional flash.
    const delta = Date.now() - start;
    if (delta < this.minLoadingAnimationMs_) {
      await new Promise(
          res => setTimeout(res, this.minLoadingAnimationMs_ - delta));
    }

    this.specsTable_ = {
      columns:
          Object.values(aggregatedDatas).map((data: AggregatedProductData) => {
            return {
              selectedItem: {
                title: data.spec ? data.spec.title : '',
                url: data.info.productUrl.url,
                imageUrl: data.info.imageUrl.url,
              },
            };
          }),
      rows,
    };
    this.showEmptyState_ = this.specsTable_.columns.length === 0;
    this.loadingState_ = {loading: false, urlCount: 0};
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

  private showTable_(): boolean {
    return !this.loadingState_.loading && !this.showEmptyState_;
  }

  private addToNewGroup_() {
    // TODO(b/330345730): Plumb through mojom
  }

  private deleteSet_() {
    if (this.id_) {
      this.shoppingApi_.deleteProductSpecificationsSet(this.id_);
    }
  }

  private updateSetName_(e: CustomEvent<{name: string}>) {
    if (this.id_) {
      this.shoppingApi_.setNameForProductSpecificationsSet(
          this.id_, e.detail.name);
    }
  }

  private seeAllSets_() {
    // TODO(b/330345730): Plumb through mojom
  }

  private onUrlAdd_(e: CustomEvent<{url: string}>) {
    const urls = this.getTableUrls_();
    urls.push(e.detail.url);
    // TODO(b/330345730): Plumb through mojom instead of calling populateTable
    // directly. Then, onSetUpdated should handle the table change.
    this.populateTable_(urls);
  }

  private onUrlChange_(e: CustomEvent<{url: string, index: number}>) {
    const urls = this.getTableUrls_();
    urls[e.detail.index] = e.detail.url;
    // TODO(b/330345730): Plumb through mojom instead of calling populateTable
    // directly. Then, onSetUpdated should handle the table change.
    this.populateTable_(urls);
  }

  private onUrlRemove_(e: CustomEvent<{index: number}>) {
    const urls = this.getTableUrls_();
    urls.splice(e.detail.index, 1);
    // TODO(b/330345730): Plumb through mojom instead of calling populateTable
    // directly. Then, onSetUpdated should handle the table change.
    this.populateTable_(urls);
  }

  private getTableUrls_(): string[] {
    return this.specsTable_.columns.map(
        (column: TableColumn) => column.selectedItem.url);
  }

  private onSetUpdated_(set: ProductSpecificationsSet) {
    if (set.uuid.value !== this.id_?.value) {
      return;
    }
    this.setName_ = set.name;
    this.populateTable_(set.urls.map(url => url.url));
  }

  private onSetRemoved_(id: Uuid) {
    if (id.value !== this.id_?.value) {
      return;
    }
    this.id_ = null;
    this.specsTable_ = {
      columns: [],
      rows: [],
    };
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'product-specifications-app': ProductSpecificationsElement;
  }
}

customElements.define(
    ProductSpecificationsElement.is, ProductSpecificationsElement);
