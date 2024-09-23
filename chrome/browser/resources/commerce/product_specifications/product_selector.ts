// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
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
      selectedItem: {
        type: Object,
        value: null,
      },

      excludedUrls: {
        type: Array,
        value: () => [],
      },
    };
  }

  selectedItem: UrlListEntry|null;
  excludedUrls: string[];

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

  private getSelectedUrl_() {
    return this.selectedItem?.url ?? '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'product-selector': ProductSelectorElement;
  }
}

customElements.define(ProductSelectorElement.is, ProductSelectorElement);
