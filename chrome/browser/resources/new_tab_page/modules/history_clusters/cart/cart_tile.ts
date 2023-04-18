// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../page_favicon.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Cart} from '../../../cart.mojom-webui.js';
import {I18nMixin} from '../../../i18n_setup.js';

import {getTemplate} from './cart_tile.html.js';

export class CartTileModuleElement extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'ntp-history-clusters-cart-tile';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /* The cart to display. */
      cart: Object,
    };
  }

  cart: Cart;

  override ready() {
    super.ready();

    if (this.cart.productImageUrls.length > 1) {
      this.setAttribute('multiple-images', 'true');
    } else {
      this.setAttribute('one-image', 'true');
    }
  }

  private hasMultipleImages_(): boolean {
    if (this.cart.productImageUrls.length > 1) {
      return true;
    } else {
      return false;
    }
  }

  private getSingleImageToShow_(): string {
    return this.cart.productImageUrls[0].url;
  }

  private getImagesToShow_(): Object[] {
    const images = this.cart.productImageUrls;
    return images.slice(0, (images.length > 4) ? 3 : 4);
  }

  private shouldShowExtraImagesCard_(): boolean {
    return this.cart.productImageUrls.length > 4;
  }

  private getExtraImagesCountString_(): string {
    return '+' + (this.cart.productImageUrls.length - 3).toString();
  }
}

customElements.define(CartTileModuleElement.is, CartTileModuleElement);
