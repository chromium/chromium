// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './product_specifications_item.js';

import type {BrowserProxy} from '//resources/cr_components/commerce/browser_proxy.js';
import {BrowserProxyImpl} from '//resources/cr_components/commerce/browser_proxy.js';
import type {ProductSpecificationsSet} from 'chrome://resources/cr_components/commerce/shopping_service.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './product_specifications_lists.html.js';

export class ProductSpecificationsListsElement extends PolymerElement {
  static get is() {
    return 'product-specifications-lists';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      allProductSpecificationsSet_: Array,
    };
  }

  private shoppingApi_: BrowserProxy = BrowserProxyImpl.getInstance();
  private allProductSpecificationsSet_: ProductSpecificationsSet[] = [];

  override async connectedCallback() {
    super.connectedCallback();
    const {sets} = await this.shoppingApi_.getAllProductSpecificationsSets();
    if (!sets) {
      return;
    }
    this.allProductSpecificationsSet_ = sets;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'product-specifications-lists': ProductSpecificationsListsElement;
  }
}

customElements.define(
    ProductSpecificationsListsElement.is, ProductSpecificationsListsElement);
