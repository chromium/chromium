// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';

import type {PriceTrackingBrowserProxy} from '//resources/cr_components/commerce/price_tracking_browser_proxy.js';
import {PriceTrackingBrowserProxyImpl} from '//resources/cr_components/commerce/price_tracking_browser_proxy.js';
import type {BookmarkProductInfo, ProductInfo} from '//resources/cr_components/commerce/shared.mojom-webui.js';
import type {PriceInsightsInfo} from '//resources/cr_components/commerce/shopping_service.mojom-webui.js';
import {PriceInsightsInfo_PriceBucket} from '//resources/cr_components/commerce/shopping_service.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './price_tracking_section.css.js';
import {getHtml} from './price_tracking_section.html.js';

export interface PriceTrackingSectionElement {
  $: {
    toggleTitle: HTMLElement,
    toggleAnnotation: HTMLElement,
    toggle: HTMLElement,
  };
}

export class PriceTrackingSectionElement extends CrLitElement {
  static get is() {
    return 'price-tracking-section';
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
      isProductTracked: {type: Boolean},
      folderName_: {type: String},
      saveLocationEndText_: {type: String},
      saveLocationStartText_: {type: String},
      showSaveLocationText_: {type: Boolean},
      toggleAnnotationText_: {type: String},
    };
  }

  accessor productInfo: ProductInfo = {
    title: '',
    clusterTitle: '',
    domain: '',
    imageUrl: '',
    productUrl: '',
    currentPrice: '',
    previousPrice: '',
    clusterId: BigInt(0),
    categoryLabels: [],
    priceSummary: '',
  };
  accessor isProductTracked: boolean = false;
  protected accessor folderName_: string = '';
  protected accessor saveLocationStartText_: string = '';
  protected accessor saveLocationEndText_: string = '';
  protected accessor showSaveLocationText_: boolean = false;
  protected accessor toggleAnnotationText_: string = '';
  priceInsightsInfo: PriceInsightsInfo = {
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

  private listenerIds_: number[] = [];
  private priceTrackingProxy_: PriceTrackingBrowserProxy =
      PriceTrackingBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    const callbackRouter = this.priceTrackingProxy_.getCallbackRouter();
    this.listenerIds_.push(
        callbackRouter.priceTrackedForBookmark.addListener(
            (product: BookmarkProductInfo) =>
                this.onBookmarkPriceTracked(product)),
        callbackRouter.priceUntrackedForBookmark.addListener(
            (product: BookmarkProductInfo) =>
                this.onBookmarkPriceUntracked(product)),
        callbackRouter.operationFailedForBookmark.addListener(
            (product: BookmarkProductInfo, attemptedTrack: boolean) =>
                this.onBookmarkOperationFailed(product, attemptedTrack)),
        callbackRouter.onProductBookmarkMoved.addListener(
            (product: BookmarkProductInfo) =>
                this.onProductBookmarkMoved(product)),
    );

    this.updatePriceTrackingSection_(this.isProductTracked);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.listenerIds_.forEach(
        id => this.priceTrackingProxy_.getCallbackRouter().removeListener(id));
  }

  private async updatePriceTrackingSection_(tracked: boolean) {
    if (!tracked) {
      this.folderName_ = '';
      this.toggleAnnotationText_ =
          loadTimeData.getString('trackPriceDescription');
    } else {
      this.folderName_ = (await this.priceTrackingProxy_
                              .getParentBookmarkFolderNameForCurrentUrl())
                             .name;

      this.toggleAnnotationText_ =
          loadTimeData.getString('trackPriceSaveDescription');
    }
    this.updateSaveLocationText(this.folderName_);
    this.isProductTracked = tracked;
  }

  private updateSaveLocationText(folderName: string) {
    if (folderName.length === 0) {
      this.showSaveLocationText_ = false;
      this.saveLocationStartText_ = '';
      this.saveLocationEndText_ = '';
      return;
    }

    const fullText: string =
        loadTimeData.getStringF('trackPriceSaveLocation', folderName);

    // TODO(crbug.com/40066115): Find a better way to dynamically add a link to a templated
    //                string and possibly avoid using substring.
    this.saveLocationStartText_ =
        fullText.substring(0, fullText.lastIndexOf(folderName));
    this.saveLocationEndText_ = fullText.substring(
        fullText.lastIndexOf(folderName) + folderName.length);
    this.showSaveLocationText_ = true;
  }

  protected onToggleCheckedChanged_(e: CustomEvent<{value: boolean}>) {
    this.isProductTracked = e.detail.value;
  }

  protected onToggleChange_() {
    this.priceTrackingProxy_.setPriceTrackingStatusForCurrentUrl(
        this.isProductTracked);
    chrome.metricsPrivate.recordEnumerationValue(
        this.isProductTracked ?
            'Commerce.PriceTracking.PriceInsightsSidePanel.Track' :
            'Commerce.PriceTracking.PriceInsightsSidePanel.Untrack',
        this.priceInsightsInfo.bucket,
        PriceInsightsInfo_PriceBucket.MAX_VALUE + 1);
  }

  private onBookmarkPriceTracked(product: BookmarkProductInfo) {
    if (product.info.clusterId !== this.productInfo.clusterId) {
      return;
    }
    this.updatePriceTrackingSection_(true);
  }

  private onBookmarkPriceUntracked(product: BookmarkProductInfo) {
    if (product.info.clusterId !== this.productInfo.clusterId) {
      return;
    }
    this.updatePriceTrackingSection_(false);
  }

  protected onToggleAnnotationButtonClick_() {
    this.priceTrackingProxy_.showBookmarkEditorForCurrentUrl();
    chrome.metricsPrivate.recordUserAction(
        'Commerce.PriceTracking.' +
        'EditedBookmarkFolderFromPriceInsightsSidePanel');
  }

  private onBookmarkOperationFailed(
      product: BookmarkProductInfo, attemptedTrack: boolean) {
    if (product.info.clusterId !== this.productInfo.clusterId) {
      return;
    }
    this.toggleAnnotationText_ = loadTimeData.getString('trackPriceError');
    this.folderName_ = '';
    this.updateSaveLocationText('');
    this.isProductTracked = !attemptedTrack;
  }

  private async onProductBookmarkMoved(product: BookmarkProductInfo) {
    if (product.info.clusterId === this.productInfo.clusterId &&
        this.isProductTracked) {
      this.folderName_ = (await this.priceTrackingProxy_
                              .getParentBookmarkFolderNameForCurrentUrl())
                             .name;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'price-tracking-section': PriceTrackingSectionElement;
  }
}

customElements.define(
    PriceTrackingSectionElement.is, PriceTrackingSectionElement);
