// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './shared_icons.html.js';
import './searched_label.js';

import type {BrowserProxy} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import {BrowserProxyImpl} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import type {ProductSpecificationsSet} from 'chrome://resources/cr_components/commerce/shopping_service.mojom-webui.js';
import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {FocusRow} from 'chrome://resources/js/focus_row.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {Uuid} from 'chrome://resources/mojo/mojo/public/mojom/base/uuid.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './product_specifications_item.html.js';

export type ItemCheckboxSelectEvent = CustomEvent<{
  checked: boolean,
  shiftKey: boolean,
  uuid: string,
  index: number,
}>;

export type ItemMenuOpenEvent = CustomEvent<{
  uuid: Uuid,
  target: HTMLElement,
}>;

export interface ProductSpecificationsItemElement {
  $: {
    'checkbox': CrCheckboxElement,
    'link': HTMLElement,
    'menu': HTMLElement,
    'url': HTMLElement,
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
      item: Object,

      checked: Boolean,

      index: Number,

      searchTerm: String,
    };
  }

  item: ProductSpecificationsSet;
  checked: boolean;
  index: number;
  private isShiftKeyDown_: boolean = false;
  private shoppingApi_: BrowserProxy = BrowserProxyImpl.getInstance();

  private onLinkClick_() {
    this.shoppingApi_.showProductSpecificationsSetForUuid(
        this.item.uuid, /*inNewTab=*/ true);
  }

  private onLinkKeydown_(event: KeyboardEvent) {
    if (event.key !== 'Enter') {
      return;
    }
    this.shoppingApi_.showProductSpecificationsSetForUuid(
        this.item.uuid, /*inNewTab=*/ true);
  }

  // This is necessary for shift-checkbox detection: preventing the mousedown
  // allows the mousedown to happen at the checkbox before the on-change.
  private onRowPointerDown_(e: PointerEvent) {
    if (e.shiftKey) {
      e.preventDefault();
    }
  }

  private fire_(eventName: string, detail?: any) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true, detail}));
  }

  private onCheckboxPointerDown_(e: PointerEvent) {
    this.isShiftKeyDown_ = e.shiftKey;
  }

  private onCheckboxChange_() {
    const detail: ItemCheckboxSelectEvent['detail'] = {
      checked: this.checked,
      shiftKey: this.isShiftKeyDown_,
      uuid: this.item.uuid.value,
      index: this.index,
    };
    this.fire_('product-spec-item-select', detail);
    this.isShiftKeyDown_ = false;
  }

  private getItemTitle_(): string {
    return loadTimeData.getStringF(
        'compareHistoryRow', this.item.name, this.item.urls.length);
  }

  private getMenuAriaLabel_(): string {
    return loadTimeData.getStringF(
        'compareHistoryMenuAriaLabel', this.item.name);
  }

  private getUrl_(): string {
    // TODO: b/353981858 - consider sending url from shopping api.
    return 'chrome://compare/?id=' + this.item.uuid.value;
  }

  createFocusRow(): FocusRow {
    const focusRow =
        new FocusRow(this.shadowRoot!.getElementById('item-container')!, null);
    focusRow.addItem('checkbox', 'cr-checkbox');
    focusRow.addItem('link', '#link');
    focusRow.addItem('menu', '#menu');
    return focusRow;
  }

  private onMenuButtonClick_(e: Event) {
    e.stopPropagation();
    // ItemMenuOpenEvent
    this.fire_('item-menu-open', {uuid: this.item.uuid, target: e.target});
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'product-specifications-item': ProductSpecificationsItemElement;
  }
}

customElements.define(
    ProductSpecificationsItemElement.is, ProductSpecificationsItemElement);
