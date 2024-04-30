// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './product_selection_menu.js';

import type {CrUrlListItemElement} from 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {ProductSelectionMenuElement} from './product_selection_menu.js';
import {getTemplate} from './product_selector.html.js';
import {getAbbreviatedUrl} from './utils.js';
import type {UrlListEntry} from './utils.js';

export interface ProductSelectorElement {
  $: {
    currentProduct: CrUrlListItemElement,
    currentProductContainer: HTMLElement,
    productSelectionMenu: ProductSelectionMenuElement,
  };
}

export class ProductSelectorElement extends PolymerElement {
  static get is() {
    return 'product-selector';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      selectedItem: Object,
    };
  }

  selectedItem: UrlListEntry;

  private showMenu_() {
    this.$.productSelectionMenu.showAt(this.$.currentProductContainer);
    this.$.currentProductContainer.classList.add('showing-menu');
  }

  private onCloseMenu_() {
    this.$.currentProductContainer.classList.remove('showing-menu');
  }

  private onCurrentProductContainerKeyDown_(e: KeyboardEvent) {
    if (e.key === ' ' || e.key === 'Enter') {
      e.preventDefault();
      e.stopPropagation();
      this.showMenu_();
    }
  }

  private getUrl_(item: UrlListEntry) {
    return getAbbreviatedUrl(item.url);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'product-selector': ProductSelectorElement;
  }
}

customElements.define(ProductSelectorElement.is, ProductSelectorElement);
