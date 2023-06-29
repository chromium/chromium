// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../strings.m.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';

import {ShoppingListApiProxy, ShoppingListApiProxyImpl} from '//shopping-insights-side-panel.top-chrome/shared/commerce/shopping_list_api_proxy.js';
import {BookmarkProductInfo, ProductInfo} from '//shopping-insights-side-panel.top-chrome/shared/shopping_list.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './price_tracking_section.html.js';

export interface PriceTrackingSection {
  $: {
    toggleTitle: HTMLElement,
    toggleAnnotation: HTMLElement,
    toggle: HTMLElement,
  };
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
  private isProductTracked_: boolean;
  private listenerIds_: number[] = [];

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
    );


    this.shoppingApi_.getPriceTrackingStatusForCurrentUrl().then(res => {
      this.isProductTracked_ = res.tracked;
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.listenerIds_.forEach(
        id => this.shoppingApi_.getCallbackRouter().removeListener(id));
  }

  private onPriceTrackingToggled_() {
    this.shoppingApi_.setPriceTrackingStatusForCurrentUrl(
        this.isProductTracked_);
  }

  private onBookmarkPriceTracked(product: BookmarkProductInfo) {
    if (product.info.clusterId !== this.productInfo.clusterId) {
      return;
    }
    this.isProductTracked_ = true;
  }

  private onBookmarkPriceUntracked(product: BookmarkProductInfo) {
    if (product.info.clusterId !== this.productInfo.clusterId) {
      return;
    }
    this.isProductTracked_ = false;
  }

  private getToggleAnnotationText_(isProductTracked: boolean) {
    if (!isProductTracked) {
      return loadTimeData.getString('trackPriceDescription');
    } else {
      return loadTimeData.getStringF('trackPriceDone', '');
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'price-tracking-section': PriceTrackingSection;
  }
}

customElements.define(PriceTrackingSection.is, PriceTrackingSection);
