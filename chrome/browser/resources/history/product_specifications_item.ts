// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import './shared_icons.html.js';

import type {ProductSpecificationsSet} from 'chrome://resources/cr_components/commerce/shopping_service.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './product_specifications_item.html.js';


export class ProductSpecificationsItemElement extends PolymerElement {
  static get is() {
    return 'product-specifications-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      productSet: Object,
    };
  }

  productSet: ProductSpecificationsSet;
}

declare global {
  interface HTMLElementTagNameMap {
    'product-specifications-item': ProductSpecificationsItemElement;
  }
}

customElements.define(
    ProductSpecificationsItemElement.is, ProductSpecificationsItemElement);
