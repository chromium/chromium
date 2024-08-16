// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';
import './header.js';
import './loading_state.js';
import './new_column_selector.js';
import './product_selector.js';
import './table.js';
import './horizontal_carousel.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import type {BrowserProxy} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import {BrowserProxyImpl} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import type {PageCallbackRouter, ProductSpecificationsSet} from 'chrome://resources/cr_components/commerce/shopping_service.mojom-webui.js';
import type {CrFeedbackButtonsElement} from 'chrome://resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import {CrFeedbackOption} from 'chrome://resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import type {Uuid} from 'chrome://resources/mojo/mojo/public/mojom/base/uuid.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import type {HeaderElement} from './header.js';
import type {NewColumnSelectorElement} from './new_column_selector.js';
import type {ProductSelectorElement} from './product_selector.js';
import {Router} from './router.js';
import type {ProductInfo, ProductSpecifications, ProductSpecificationsDescriptionText, ProductSpecificationsProduct} from './shopping_service.mojom-webui.js';
import {UserFeedback} from './shopping_service.mojom-webui.js';
import type {TableElement} from './table.js';
import type {UrlListEntry} from './utils.js';
import {WindowProxy} from './window_proxy.js';

interface AggregatedProductData {
  info: ProductInfo|null;
  spec: ProductSpecificationsProduct|null;
}

interface LoadingState {
  loading: boolean;
  urlCount: number;
}

interface Description {
  label: string;
  description: string;
}

interface ProductDetail {
  title: string;
  // Only one of `text` or `description` will be set at a time.
  text: string|null;
  description: Description[];
  summary: ProductSpecificationsDescriptionText[];
}

export interface TableColumn {
  selectedItem: UrlListEntry;
  productDetails: ProductDetail[]|null;
}

export interface ProductSpecificationsElement {
  $: {
    feedbackButtons: CrFeedbackButtonsElement,
    empty: HTMLElement,
    header: HeaderElement,
    loading: HTMLElement,
    newColumnSelector: NewColumnSelectorElement,
    offlineToast: CrToastElement,
    productSelector: ProductSelectorElement,
    specs: HTMLElement,
    summaryTable: TableElement,
  };
}

function getProductDetails(
    product: ProductSpecificationsProduct|null,
    productSpecs: ProductSpecifications,
    productInfo: ProductInfo|null): ProductDetail[] {
  const productDetails: ProductDetail[] = [];

  // First add rows that don't come directly from the product
  // specifications backend.
  productDetails.push({
    title: loadTimeData.getString('priceRowTitle'),
    text: productInfo?.currentPrice || null,
    description: [],
    summary: [],
  });

  // The second row is the product-level summary.
  productDetails.push({
    title: loadTimeData.getString('productSummaryRowTitle'),
    text: null,
    description: [],
    summary: product?.summary || [],
  });

  productSpecs.productDimensionMap.forEach((title: string, key: bigint) => {
    if (!product) {
      // Fill missing product details with strings to ensure uniform table row
      // count.
      productDetails.push({title, text: null, description: [], summary: []});
    } else {
      const value = product.productDimensionValues.get(key);
      const description =
          (value?.specificationDescriptions || []).flatMap(description => {
            return {
              label: description.label,
              description:
                  description.options.flatMap(option => option.descriptions)
                      .flatMap(desc => desc.text)
                      .join(', '),
            };
          }) ||
          [];
      const summary = value?.summary || [];
      productDetails.push({title, text: null, description, summary});
    }
  });
  return productDetails;
}

function findProductInResults(clusterId: bigint, specs: ProductSpecifications):
    ProductSpecificationsProduct|null {
  if (!specs) {
    return null;
  }

  for (const product of specs.products) {
    if (product.productClusterId.toString() === clusterId.toString()) {
      return product;
    }
  }

  return null;
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
      tableColumns_: Object,
    };
  }

  private loadingState_: LoadingState = {loading: false, urlCount: 0};
  private setName_: string|null = null;
  private showEmptyState_: boolean;
  private tableColumns_: TableColumn[] = [];

  private callbackRouter_: PageCallbackRouter;
  private eventTracker_: EventTracker = new EventTracker();
  private id_: Uuid|null = null;
  private listenerIds_: number[] = [];
  private minLoadingAnimationMs_: number = 500;
  private shoppingApi_: BrowserProxy = BrowserProxyImpl.getInstance();

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

    this.eventTracker_.add(
        this, 'click',
        () => {
          this.$.offlineToast.hide();
        },
        /*useCapture=*/ true);
    this.eventTracker_.add(window, 'online', () => {
      this.$.offlineToast.hide();
    });

    if (this.isOffline_) {
      this.showEmptyState_ = true;
      this.showOfflineToast_();
      return;
    }

    const router = Router.getInstance();
    const params = new URLSearchParams(router.getCurrentQuery());
    const idParam = params.get('id');
    if (idParam) {
      this.id_ = {value: idParam};
      const {set} = await this.shoppingApi_.getProductSpecificationsSetByUuid(
          {value: idParam});
      if (set) {
        document.title = set.name;
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

    // TODO(b/346601645): Detect if a set already exists
    await this.createNewSet_(urls);
  }

  resetMinLoadingAnimationMsForTesting(newValue = 0) {
    this.minLoadingAnimationMs_ = newValue;
  }

  private showOfflineToast_() {
    this.$.offlineToast.show();
  }

  private async populateTable_(urls: string[]) {
    const start = Date.now();
    this.showEmptyState_ = false;
    this.loadingState_ = {loading: true, urlCount: urls.length};

    const tableColumns: TableColumn[] = [];
    if (urls.length) {
      const {productSpecs} =
          await this.shoppingApi_.getProductSpecificationsForUrls(
              urls.map(url => ({url})));
      const aggregatedDataByUrl =
          await this.aggregateProductDataByUrl_(urls, productSpecs);

      urls.map((url: string) => {
        const info = aggregatedDataByUrl.get(url)?.info;
        const product = aggregatedDataByUrl.get(url)?.spec;

        tableColumns.push({
          selectedItem: {
            title: product?.title || info?.title || '',
            url: url,
            imageUrl: info?.imageUrl?.url || product?.imageUrl?.url || '',
          },
          productDetails:
              getProductDetails(product || null, productSpecs, info || null),
        });
      });
    }

    // Enforce a minimum amount of time in the loading state to avoid it
    // appearing like an unintentional flash.
    const delta = Date.now() - start;
    if (delta < this.minLoadingAnimationMs_ && urls.length > 0) {
      await new Promise(
          res => setTimeout(res, this.minLoadingAnimationMs_ - delta));
    }

    this.tableColumns_ = tableColumns;
    this.showEmptyState_ = this.tableColumns_.length === 0;
    this.loadingState_ = {loading: false, urlCount: 0};
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds_.forEach(id => this.callbackRouter_.removeListener(id));
    this.eventTracker_.removeAll();
  }

  private get isOffline_(): boolean {
    return !WindowProxy.getInstance().onLine;
  }

  private async getInfoForUrls_(urls: string[]):
      Promise<Map<string, ProductInfo>> {
    const infoMap: Map<string, ProductInfo> = new Map();
    for (const url of urls) {
      const {productInfo} = await this.shoppingApi_.getProductInfoForUrl({url});
      if (productInfo && productInfo.clusterId) {
        infoMap.set(url, productInfo);
      }
    }
    return infoMap;
  }

  private async aggregateProductDataByUrl_(
      urls: string[], specs: ProductSpecifications):
      Promise<Map<string, AggregatedProductData>> {
    const urlToInfoMap: Map<string, ProductInfo> =
        await this.getInfoForUrls_(urls);
    const specProductMap: Map<string, ProductSpecificationsProduct> = new Map();
    urlToInfoMap.forEach((value, key) => {
      const product = findProductInResults(value.clusterId, specs);
      if (product) {
        specProductMap.set(key, product);
      }
    });

    const aggregatedDatas: Map<string, AggregatedProductData> = new Map();
    urls.forEach((url) => {
      const productInfo = urlToInfoMap.get(url);
      const productSpecs = specProductMap.get(url);
      aggregatedDatas.set(url, {
        info: productInfo ? productInfo : null,
        spec: productSpecs ? productSpecs : null,
      });
    });
    return aggregatedDatas;
  }

  private showTable_(): boolean {
    return !this.loadingState_.loading && !this.showEmptyState_;
  }

  private deleteSet_() {
    if (this.isOffline_) {
      this.showOfflineToast_();
      return;
    }

    if (this.id_) {
      this.shoppingApi_.deleteProductSpecificationsSet(this.id_);
    }
  }

  private updateSetName_(e: CustomEvent<{name: string}>) {
    if (this.isOffline_) {
      this.showOfflineToast_();
      return;
    }

    if (this.id_) {
      this.shoppingApi_.setNameForProductSpecificationsSet(
          this.id_, e.detail.name);
    }
  }

  private seeAllSets_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('productSpecificationsManagementUrl'));
  }

  private async onUrlAdd_(e: CustomEvent<{url: string}>) {
    if (this.isOffline_) {
      this.showOfflineToast_();
      return;
    }
    const urls = this.getTableUrls_();
    urls.push(e.detail.url);
    // If there is already a current set, we won't be showing the disclosure and
    // we can modify the set directly; otherwise, user is trying to add a url
    // from empty state, and we'll try to show the disclosure.
    if (this.id_) {
      this.modifyUrls_(urls);
      return;
    }
    const {disclosureShown} =
        await this.shoppingApi_.maybeShowProductSpecificationDisclosure(
            urls.map(url => ({url})), this.setName_ ? this.setName_ : '');
    // If the disclosure is shown, we won't update the current set.
    if (!disclosureShown) {
      this.modifyUrls_(urls);
    }
  }

  private onUrlChange_(e: CustomEvent<{url: string, index: number}>) {
    if (this.isOffline_) {
      this.showOfflineToast_();
      return;
    }

    const urls = this.getTableUrls_();
    urls[e.detail.index] = e.detail.url;
    this.modifyUrls_(urls);
  }

  private onUrlOrderUpdate_() {
    if (this.isOffline_) {
      this.showOfflineToast_();
      return;
    }

    const urls = this.getTableUrls_();
    this.modifyUrls_(urls);
  }

  private onUrlRemove_(e: CustomEvent<{index: number}>) {
    if (this.isOffline_) {
      this.showOfflineToast_();
      return;
    }

    const urls = this.getTableUrls_();
    urls.splice(e.detail.index, 1);
    this.modifyUrls_(urls);
  }

  private modifyUrls_(urls: string[]) {
    if (this.id_) {
      this.shoppingApi_.setUrlsForProductSpecificationsSet(
          this.id_!, urls.map(url => ({url})));
    } else {
      this.createNewSet_(urls);
    }
  }

  private async createNewSet_(urls: string[]) {
    if (this.isOffline_) {
      this.showOfflineToast_();
      return;
    }

    assert(!this.id_ && !this.setName_);
    // TODO(b/346381503): Use a more targeted set name.
    this.setName_ = 'Product specs';
    const {createdSet} = await this.shoppingApi_.addProductSpecificationsSet(
        this.setName_, urls.map(url => ({url})));
    if (createdSet) {
      this.id_ = createdSet.uuid;
      window.history.replaceState(undefined, '', `?id=${this.id_.value}`);
    }
    this.populateTable_(urls);
  }

  private getTableUrls_(): string[] {
    return this.tableColumns_.map(
        (column: TableColumn) => column.selectedItem.url);
  }

  private onSetUpdated_(set: ProductSpecificationsSet) {
    if (set.uuid.value !== this.id_?.value) {
      return;
    }
    document.title = set.name;
    this.setName_ = set.name;

    let urlSetChanged = false;
    let orderChanged = false;
    const tableUrls = this.getTableUrls_();

    if (tableUrls.length === set.urls.length) {
      for (const [i, setUrl] of set.urls.entries()) {
        if (setUrl.url !== tableUrls[i]) {
          orderChanged = true;
        }

        if (!tableUrls.includes(setUrl.url)) {
          urlSetChanged = true;
          break;
        }
      }
    } else {
      urlSetChanged = true;
    }

    if (urlSetChanged) {
      this.populateTable_(set.urls.map(url => url.url));
    } else if (orderChanged) {
      const newCols: TableColumn[] = [];

      for (const [_, setUrl] of set.urls.entries()) {
        const existingIndex = tableUrls.indexOf(setUrl.url);
        assert(existingIndex >= 0, 'Did not find column to reorder!');

        newCols.push(this.tableColumns_[existingIndex]);
      }

      this.tableColumns_ = newCols;
    }
  }

  private onSetRemoved_(id: Uuid) {
    if (id.value === this.id_?.value) {
      window.location.replace(window.location.origin);
    }
  }

  private onFeedbackSelectedOptionChanged_(
      e: CustomEvent<{value: CrFeedbackOption}>) {
    switch (e.detail.value) {
      case CrFeedbackOption.UNSPECIFIED:
        this.shoppingApi_.setProductSpecificationsUserFeedback(
            UserFeedback.kUnspecified);
        return;
      case CrFeedbackOption.THUMBS_UP:
        this.shoppingApi_.setProductSpecificationsUserFeedback(
            UserFeedback.kThumbsUp);
        return;
      case CrFeedbackOption.THUMBS_DOWN:
        this.shoppingApi_.setProductSpecificationsUserFeedback(
            UserFeedback.kThumbsDown);
        return;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'product-specifications-app': ProductSpecificationsElement;
  }
}

customElements.define(
    ProductSpecificationsElement.is, ProductSpecificationsElement);
