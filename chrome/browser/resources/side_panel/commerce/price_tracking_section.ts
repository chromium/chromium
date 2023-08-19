// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../strings.m.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';

import {ShoppingListApiProxy, ShoppingListApiProxyImpl} from '//shopping-insights-side-panel.top-chrome/shared/commerce/shopping_list_api_proxy.js';
import {BookmarkProductInfo, PriceInsightsInfo, PriceInsightsInfo_PriceBucket, ProductInfo} from '//shopping-insights-side-panel.top-chrome/shared/shopping_list.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
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

      isProductTracked_: {
        type: Boolean,
        value: false,
      },
    };
  }

  productInfo: ProductInfo;
  priceInsightsInfo: PriceInsightsInfo;
  private isProductTracked_: boolean;
  private listenerIds_: number[] = [];
  private toggleAnnotationText_: string;
  private folderName_: string;

  private shoppingApi_: ShoppingListApiProxy =
      ShoppingListApiProxyImpl.getInstance();

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
    );


    this.shoppingApi_.getPriceTrackingStatusForCurrentUrl().then(res => {
      this.updatePriceTrackingSection_(res.tracked);
    });
  }

  private async updatePriceTrackingSection_(tracked: boolean) {
    if (!tracked) {
      this.folderName_ = '';
      // TODO(crbug.com/1456420): Update the string to include the period.
      this.toggleAnnotationText_ =
          loadTimeData.getString('trackPriceDescription');
    } else {
      const {name} =
          await this.shoppingApi_.getParentBookmarkFolderNameForCurrentUrl();
      this.folderName_ = decodeString16(name);
      this.toggleAnnotationText_ =
          loadTimeData.getStringF('trackPriceDone', '');
    }
    this.isProductTracked_ = tracked;
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.listenerIds_.forEach(
        id => this.shoppingApi_.getCallbackRouter().removeListener(id));
  }

  private onPriceTrackingToggled_() {
    this.shoppingApi_.setPriceTrackingStatusForCurrentUrl(
        this.isProductTracked_);
    chrome.metricsPrivate.recordEnumerationValue(
        this.isProductTracked_ ?
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
    this.isProductTracked_ = !attemptedTrack;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'price-tracking-section': PriceTrackingSection;
  }
}

customElements.define(PriceTrackingSection.is, PriceTrackingSection);
