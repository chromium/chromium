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
import './shared_vars.css.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import type {BrowserProxy} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import {BrowserProxyImpl} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import type {PageCallbackRouter, ProductSpecificationsFeatureState, ProductSpecificationsSet} from 'chrome://resources/cr_components/commerce/shopping_service.mojom-webui.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrFeedbackOption} from 'chrome://resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import type {Uuid} from 'chrome://resources/mojo/mojo/public/mojom/base/uuid.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import type {BuyingOptionsLink} from './buying_options_section.js';
import type {ProductDescription} from './description_section.js';
import type {HeaderElement} from './header.js';
import type {NewColumnSelectorElement} from './new_column_selector.js';
import {SectionType} from './product_selection_menu.js';
import type {ProductSelectorElement} from './product_selector.js';
import {Router} from './router.js';
import type {ProductInfo, ProductSpecifications, ProductSpecificationsProduct} from './shopping_service.mojom-webui.js';
import {UserFeedback} from './shopping_service.mojom-webui.js';
import type {TableElement} from './table.js';
import type {UrlListEntry} from './utils.js';
import {WindowProxy} from './window_proxy.js';

interface AggregatedProductData {
  productInfo: ProductInfo|null;
  spec: ProductSpecificationsProduct|null;
}

interface LoadingState {
  loading: boolean;
  urlCount: number;
}

export type Content = string|ProductDescription|BuyingOptionsLink|null;

interface ProductDetail {
  title: string|null;
  content: Content;
}

export interface TableColumn {
  selectedItem: UrlListEntry;
  productDetails: ProductDetail[]|null;
}

export interface ProductSpecificationsElement {
  $: {
    empty: HTMLElement,
    error: HTMLElement,
    errorToast: CrToastElement,
    header: HeaderElement,
    loading: HTMLElement,
    newColumnSelector: NewColumnSelectorElement,
    offlineToast: CrToastElement,
    productSelector: ProductSelectorElement,
    specs: HTMLElement,
    summaryContainer: HTMLElement,
    summaryTable: TableElement,
    syncPromo: HTMLElement,
    turnOnSyncButton: CrButtonElement,
  };
}

// This enum is used for metrics and should be kept in sync with the enum of
// the same name in enums.xml.
export enum CompareTableColumnAction {
  REMOVE = 0,
  CHANGE_ORDER_DRAG_AND_DROP = 1,
  ADD_FROM_SUGGESTED = 2,
  UPDATE_FROM_SUGGESTED = 3,
  ADD_FROM_RECENTLY_VIEWED = 4,
  UPDATE_FROM_RECENTLY_VIEWED = 5,
  // Must be last:
  MAX_VALUE = 6,
}

export const COLUMN_MODIFICATION_HISTOGRAM_NAME: string =
    'Commerce.Compare.Table.ColumnModification';

enum AppState {
  ERROR = 0,
  TABLE_EMPTY = 1,
  SYNC_SCREEN = 2,
  TABLE_POPULATED = 3,
  LOADING = 4,
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
    content: productInfo?.currentPrice || null,
  });

  // The second row is the product-level summary.
  productDetails.push({
    title: loadTimeData.getString('productSummaryRowTitle'),
    content: {
      attributes: [],
      summary: product?.summary || [],
    },
  });

  productSpecs.productDimensionMap.forEach((title: string, key: bigint) => {
    if (!product) {
      // Fill in missing product details to ensure uniform table row count.
      productDetails.push({title, content: null});
    } else {
      const value = product.productDimensionValues.get(key);
      const attributes =
          (value?.specificationDescriptions || []).flatMap(description => {
            return {
              label: description.label,
              value: description.options.flatMap(option => option.descriptions)
                         .flatMap(desc => desc.text)
                         .join(', '),
            };
          }) ||
          [];
      const summary = value?.summary || [];
      productDetails.push({
        title,
        content: {
          attributes,
          summary,
        },
      });
    }
  });

  // The last row is buying options.
  productDetails.push({
    title: null,
    content: {
      jackpotUrl: product?.buyingOptionsUrl.url || '',
    },
  });

  return productDetails;
}

function areStatesEqual(
    firstState: ProductSpecificationsFeatureState,
    secondState: ProductSpecificationsFeatureState) {
  return firstState.isSyncingTabCompare === secondState.isSyncingTabCompare &&
      firstState.canLoadFullPageUi === secondState.canLoadFullPageUi &&
      firstState.canManageSets === secondState.canManageSets &&
      firstState.canFetchData === secondState.canFetchData &&
      firstState.isAllowedForEnterprise === secondState.isAllowedForEnterprise;
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
      appState_: {
        type: Object,
        computed: 'computeAppState_(productSpecificationsFeatureState_.*,' +
            ' loadingState_.loading, showEmptyState_)',
      },
      loadingState_: Object,
      setName_: String,
      showTableDataUnavailableContainer_: {
        type: Boolean,
        computed: 'computeShowTableDataUnavailableContainer_(appState_)',
        reflectToAttribute: true,
      },
      tableColumns_: Object,
    };
  }

  private appState_: AppState = AppState.LOADING;
  private loadingState_: LoadingState = {loading: false, urlCount: 0};
  private setName_: string|null = null;
  private showTableDataUnavailableContainer_: boolean;
  private tableColumns_: TableColumn[] = [];

  private callbackRouter_: PageCallbackRouter;
  private eventTracker_: EventTracker = new EventTracker();
  private id_: Uuid|null = null;
  private listenerIds_: number[] = [];
  private loadingAnimationSlidePx_: number = 16;
  private loadingAnimationSlideDurationMs_: number = 200;
  private minLoadingAnimationMs_: number = 500;
  private productSpecificationsFeatureState_: ProductSpecificationsFeatureState;
  private shoppingApi_: BrowserProxy = BrowserProxyImpl.getInstance();
  private showEmptyState_: boolean;

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

    // TODO: b/358131415 - use listeners to update. Temporary workaround uses
    // window focus to update the feature state, to check signin.
    window.addEventListener('focus', async () => {
      const previousState = this.productSpecificationsFeatureState_;
      const {state} =
          await this.shoppingApi_.getProductSpecificationsFeatureState();
      if (!state || areStatesEqual(previousState, state)) {
        return;
      }

      // States have changed, so we need to reload the table.
      // Update the featureState after loadTable_(), so that the loading
      // state will animate first.
      await this.loadTable_(state);
      this.productSpecificationsFeatureState_ = state;
    });

    this.eventTracker_.add(
        this, 'click',
        () => {
          this.$.offlineToast.hide();
          this.$.errorToast.hide();
        },
        /*useCapture=*/ true);
    this.eventTracker_.add(window, 'online', () => {
      this.$.offlineToast.hide();
    });

    if (this.isOffline_) {
      this.showOfflineToast_();
      return;
    }

    // TODO(b/358131415): update after we use listener/ observers and no longer
    // need the featureState
    const {state} =
        await this.shoppingApi_.getProductSpecificationsFeatureState();
    if (!state) {
      return;
    }
    await this.loadTable_(state);
    this.productSpecificationsFeatureState_ = state;
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds_.forEach(id => this.callbackRouter_.removeListener(id));
    this.eventTracker_.removeAll();
  }

  // TODO(b/364337413): update tests to not rely on animation rendering time
  resetMinLoadingAnimationMsForTesting(newValue = 0) {
    this.minLoadingAnimationMs_ = newValue;
  }

  private async loadTable_(state: ProductSpecificationsFeatureState) {
    // Don't load the table if access conditions are not met.
    if (!(state.isSyncingTabCompare && state.canLoadFullPageUi &&
          state.canFetchData && state.isAllowedForEnterprise)) {
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
        const {disclosureShown} =
            await this.shoppingApi_.maybeShowProductSpecificationDisclosure(
                /* urls= */[], /* name= */ '', idParam);
        if (disclosureShown) {
          this.showEmptyState_ = true;
          this.id_ = null;
          return;
        }
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

  private computeAppState_() {
    if (this.productSpecificationsFeatureState_) {
      if (!this.productSpecificationsFeatureState_.isSyncingTabCompare) {
        return AppState.SYNC_SCREEN;
      }
      if (!(this.productSpecificationsFeatureState_.canLoadFullPageUi &&
            this.productSpecificationsFeatureState_.canFetchData &&
            this.productSpecificationsFeatureState_.isAllowedForEnterprise)) {
        return AppState.ERROR;
      }
      if (this.loadingState_.loading) {
        return AppState.LOADING;
      }
      if (this.showEmptyState_) {
        return AppState.TABLE_EMPTY;
      }
      return AppState.TABLE_POPULATED;
    }
    return AppState.ERROR;
  }

  private isAppStateError_() {
    return this.appState_ === AppState.ERROR;
  }

  private isAppStateTableEmpty_() {
    return this.appState_ === AppState.TABLE_EMPTY;
  }

  private isAppStateSyncScreen_() {
    return this.appState_ === AppState.SYNC_SCREEN;
  }

  private isAppStateTablePopulated_() {
    return this.appState_ === AppState.TABLE_POPULATED;
  }

  private isAppStateLoading_() {
    return this.appState_ === AppState.LOADING;
  }

  private computeShowTableDataUnavailableContainer_() {
    return this.appState_ === AppState.ERROR ||
        this.appState_ === AppState.TABLE_EMPTY ||
        this.appState_ === AppState.SYNC_SCREEN;
  }

  private canShowFeedbackButtons_() {
    return Boolean(
        this.productSpecificationsFeatureState_?.isQualityLoggingAllowed);
  }

  private showSyncSetupFlow_() {
    assert(this.productSpecificationsFeatureState_);
    assert(!this.productSpecificationsFeatureState_.isSyncingTabCompare);

    // If user's already signed in at the account level, then user needs to turn
    // on the compare-specific sync from settings.
    if (this.productSpecificationsFeatureState_.isSignedIn) {
      OpenWindowProxyImpl.getInstance().openUrl(
          'chrome://settings/syncSetup/advanced');
      return;
    }
    this.shoppingApi_.showSyncSetupFlow();
  }

  private showOfflineToast_() {
    this.$.offlineToast.show();
  }

  private async populateTable_(urls: string[]) {
    this.$.errorToast.hide();

    // Transition directly to the empty state if there are no URLs.
    if (urls.length === 0) {
      this.tableColumns_ = [];
      this.showEmptyState_ = true;
      return;
    }

    await this.enterLoadingState_(urls.length);

    const start = Date.now();
    this.showEmptyState_ = false;

    const tableColumns: TableColumn[] = [];
    if (urls.length) {
      const {productSpecs} =
          await this.shoppingApi_.getProductSpecificationsForUrls(
              urls.map(url => ({url})));
      const aggregatedDataByUrl =
          await this.aggregateProductDataByUrl_(urls, productSpecs);


      await Promise.all(urls.map(async (url: string) => {
        const info = aggregatedDataByUrl.get(url)?.productInfo;
        const product = aggregatedDataByUrl.get(url)?.spec;
        const title = product?.title || info?.title ||
            (await this.shoppingApi_.getPageTitleFromHistory({url})).title;

        tableColumns.push({
          selectedItem: {
            title,
            url,
            imageUrl: info?.imageUrl?.url || product?.imageUrl?.url || '',
          },
          productDetails:
              getProductDetails(product || null, productSpecs, info || null),
        });
      }));

      // Show an error message if we didn't get back any dimensions. Note that
      // the URLs in the comparison will still be displayed as columns.
      if (productSpecs.productDimensionMap.size === 0 && urls.length > 1) {
        this.$.errorToast.show();
      }
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
    this.exitLoadingState_();
  }

  private get isOffline_(): boolean {
    return !WindowProxy.getInstance().onLine;
  }

  private async getProductInfoForUrls_(urls: string[]):
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
    const urlToProductInfoMap: Map<string, ProductInfo> =
        await this.getProductInfoForUrls_(urls);
    const specProductMap: Map<string, ProductSpecificationsProduct> = new Map();
    urlToProductInfoMap.forEach((value, key) => {
      const product = findProductInResults(value.clusterId, specs);
      if (product) {
        specProductMap.set(key, product);
      }
    });

    const aggregatedDatas: Map<string, AggregatedProductData> = new Map();
    urls.forEach((url) => {
      const productInfo = urlToProductInfoMap.get(url);
      const productSpecs = specProductMap.get(url);
      aggregatedDatas.set(url, {
        productInfo: productInfo ?? null,
        spec: productSpecs ?? null,
      });
    });
    return aggregatedDatas;
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

  private async onUrlAdd_(
      e: CustomEvent<{url: string, urlSection: SectionType}>) {
    if (this.isOffline_) {
      this.showOfflineToast_();
      return;
    }

    let recordValue = CompareTableColumnAction.MAX_VALUE;
    switch (e.detail.urlSection) {
      case SectionType.RECENT:
        recordValue = CompareTableColumnAction.ADD_FROM_RECENTLY_VIEWED;
        break;
      case SectionType.SUGGESTED:
        recordValue = CompareTableColumnAction.ADD_FROM_SUGGESTED;
        break;
    }
    chrome.metricsPrivate.recordEnumerationValue(
        COLUMN_MODIFICATION_HISTOGRAM_NAME, recordValue,
        CompareTableColumnAction.MAX_VALUE);

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
            urls.map(url => ({url})), this.setName_ ? this.setName_ : '',
            /* set_id= */ '');
    // If the disclosure is shown, we won't update the current set.
    if (!disclosureShown) {
      this.modifyUrls_(urls);
    }
  }

  private onUrlChange_(
      e: CustomEvent<{url: string, urlSection: SectionType, index: number}>) {
    if (this.isOffline_) {
      this.showOfflineToast_();
      return;
    }

    let recordValue = CompareTableColumnAction.MAX_VALUE;
    switch (e.detail.urlSection) {
      case SectionType.RECENT:
        recordValue = CompareTableColumnAction.UPDATE_FROM_RECENTLY_VIEWED;
        break;
      case SectionType.SUGGESTED:
        recordValue = CompareTableColumnAction.UPDATE_FROM_SUGGESTED;
        break;
    }
    chrome.metricsPrivate.recordEnumerationValue(
        COLUMN_MODIFICATION_HISTOGRAM_NAME, recordValue,
        CompareTableColumnAction.MAX_VALUE);

    const urls = this.getTableUrls_();
    urls[e.detail.index] = e.detail.url;
    this.modifyUrls_(urls);
  }

  private onUrlOrderUpdate_() {
    if (this.isOffline_) {
      this.showOfflineToast_();
      return;
    }

    chrome.metricsPrivate.recordEnumerationValue(
        COLUMN_MODIFICATION_HISTOGRAM_NAME,
        CompareTableColumnAction.CHANGE_ORDER_DRAG_AND_DROP,
        CompareTableColumnAction.MAX_VALUE);

    const urls = this.getTableUrls_();
    this.modifyUrls_(urls);
  }

  private onUrlRemove_(e: CustomEvent<{index: number}>) {
    if (this.isOffline_) {
      this.showOfflineToast_();
      return;
    }

    chrome.metricsPrivate.recordEnumerationValue(
        COLUMN_MODIFICATION_HISTOGRAM_NAME, CompareTableColumnAction.REMOVE,
        CompareTableColumnAction.MAX_VALUE);

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
    this.setName_ = loadTimeData.getString('defaultTableTitle');
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

  private isTableFull_(columnCount: number): boolean {
    return columnCount >= loadTimeData.getInteger('maxTableSize');
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

  private getDisclaimerText_(): string {
    return loadTimeData.getStringF(
        'experimentalFeatureDisclaimer', loadTimeData.getString('userEmail'));
  }

  private fadeAndSlideOutSummaryContainer_(): Animation {
    return this.$.summaryContainer.animate(
        [
          {opacity: 1, transform: 'translateY(0px)'},
          {
            opacity: 0,
            transform: `translateY(-${this.loadingAnimationSlidePx_}px)`,
          },
        ],
        {
          duration: this.loadingAnimationSlideDurationMs_,
          easing: 'ease-out',
          fill: 'forwards',
        });
  }

  private fadeAndSlideInSummaryContainer_(): Animation {
    return this.$.summaryContainer.animate(
        [
          {
            opacity: 0,
            transform: `translateY(${this.loadingAnimationSlidePx_}px)`,
          },
          {opacity: 1, transform: 'translateY(0px)'},
        ],
        {
          duration: this.loadingAnimationSlideDurationMs_,
          easing: 'ease-out',
          fill: 'forwards',
        });
  }

  // Resolves upon updating the loading state.
  private async enterLoadingState_(urlCount: number): Promise<void> {
    if ([AppState.ERROR, AppState.SYNC_SCREEN, AppState.LOADING].includes(
            this.appState_)) {
      this.loadingState_ = {loading: true, urlCount};
      return Promise.resolve();
    }

    const anim = this.fadeAndSlideOutSummaryContainer_();
    return new Promise<void>(resolve => {
      anim.addEventListener('finish', () => {
        this.loadingState_ = {loading: true, urlCount};
        resolve();
        this.fadeAndSlideInSummaryContainer_();
      });
    });
  }

  private exitLoadingState_() {
    const anim = this.fadeAndSlideOutSummaryContainer_();
    anim.addEventListener('finish', () => {
      this.loadingState_ = {loading: false, urlCount: 0};
      this.fadeAndSlideInSummaryContainer_();
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'product-specifications-app': ProductSpecificationsElement;
  }
}

customElements.define(
    ProductSpecificationsElement.is, ProductSpecificationsElement);
