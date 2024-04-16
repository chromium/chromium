// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../strings.m.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';

import type {BrowserProxy} from '//resources/cr_components/commerce/browser_proxy.js';
import {BrowserProxyImpl} from '//resources/cr_components/commerce/browser_proxy.js';
import type {BookmarkProductInfo, PriceInsightsInfo, ProductInfo} from '//resources/cr_components/commerce/shopping_service.mojom-webui.js';
import {PriceInsightsInfo_PriceBucket} from '//resources/cr_components/commerce/shopping_service.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './price_tracking_section.html.js';

export interface PriceTrackingSection {
  $: {
    toggleTitle: HTMLElement,
    toggleAnnotation: HTMLElement,
    toggle: HTMLElement,
  };
}

function decodeString16(arr: String16) {
  return arr.data.map(ch => String.fromCodePoint(ch)).join('');
}

export class PriceTrackingSection extends PolymerElement {
  static get is() {
    return 'price-tracking-section';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      productInfo: Object,

      isProductTracked: {
        type: Boolean,
        value: false,
      },
    };
  }

  productInfo: ProductInfo;
  priceInsightsInfo: PriceInsightsInfo;
  isProductTracked: boolean;
  private listenerIds_: number[] = [];
  private toggleAnnotationText_: string;
  private saveLocationStartText_: string;
  private saveLocationEndText_: string;
  private showSaveLocationText_: boolean;
  private folderName_: string;

  private shoppingApi_: BrowserProxy = BrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    const callbackRouter = this.shoppingApi_.getCallbackRouter();
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

  private async updatePriceTrackingSection_(tracked: boolean) {
    if (!tracked) {
      this.folderName_ = '';
      this.toggleAnnotationText_ =
          loadTimeData.getString('trackPriceDescription');
    } else {
      const {name} =
          await this.shoppingApi_.getParentBookmarkFolderNameForCurrentUrl();
      this.folderName_ = decodeString16(name);

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

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.listenerIds_.forEach(
        id => this.shoppingApi_.getCallbackRouter().removeListener(id));
  }

  private onPriceTrackingToggled_() {
    this.shoppingApi_.setPriceTrackingStatusForCurrentUrl(
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

  private onFolderClicked_() {
    this.shoppingApi_.showBookmarkEditorForCurrentUrl();
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
      const {name} =
          await this.shoppingApi_.getParentBookmarkFolderNameForCurrentUrl();
      this.folderName_ = decodeString16(name);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'price-tracking-section': PriceTrackingSection;
  }
}

customElements.define(PriceTrackingSection.is, PriceTrackingSection);
