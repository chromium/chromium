// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import './shared_icons.html.js';

import type {BrowserProxy} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import {BrowserProxyImpl} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import type {ProductSpecificationsSet} from 'chrome://resources/cr_components/commerce/shopping_service.mojom-webui.js';
import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {FocusRow} from 'chrome://resources/js/focus_row.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './product_specifications_item.html.js';

export type ItemCheckboxSelectEvent = CustomEvent<{
  checked: boolean,
  uuid: string,
}>;

export interface ProductSpecificationsItemElement {
  $: {
    'checkbox': CrCheckboxElement,
    'link': HTMLElement,
  };
}

export class ProductSpecificationsItemElement extends PolymerElement {
  static get is() {
    return 'product-specifications-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      productSet: {
        type: Object,
      },

      checked_: Boolean,
    };
  }

  productSet: ProductSpecificationsSet;

  private checked_: boolean;

  private shoppingApi_: BrowserProxy = BrowserProxyImpl.getInstance();

  // TODO: b/335670350 - add method for opening the set, based on uuid.

  private onRowPointerDown_(e: PointerEvent) {
    // Prevent shift clicking a checkbox from selecting text.
    if (e.shiftKey) {
      e.preventDefault();
    }
  }

  private fire_(eventName: string, detail?: any) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true, detail}));
  }

  private onCheckboxChange_(e: MouseEvent) {
    // Prevent shift clicking a checkbox from selecting text.
    if (e.shiftKey) {
      e.preventDefault();
    }
    this.fire_(
        'item-checkbox-select',
        {checked: this.checked_, uuid: this.productSet.uuid.value});
  }

  private getItemTitle_(): string {
    return loadTimeData.getStringF(
        'productSpecificationsRow', this.productSet.name,
        this.productSet.urls.length);
  }

  createFocusRow(): FocusRow {
    const focusRow =
        new FocusRow(this.shadowRoot!.getElementById('item-container')!, null);
    focusRow.addItem('checkbox', 'cr-checkbox');
    focusRow.addItem('link', '#link');
    return focusRow;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'product-specifications-item': ProductSpecificationsItemElement;
  }
}

customElements.define(
    ProductSpecificationsItemElement.is, ProductSpecificationsItemElement);
