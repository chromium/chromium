// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import './product_selection_menu.js';

import type {CrUrlListItemElement} from 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ProductSelectionMenuElement} from './product_selection_menu.js';
import {getCss} from './product_selector.css.js';
import {getHtml} from './product_selector.html.js';
import {getAbbreviatedUrl} from './utils.js';
import type {UrlListEntry} from './utils.js';

export interface ProductSelectorElement {
  $: {
    currentProduct: CrUrlListItemElement,
    currentProductContainer: HTMLElement,
    productSelectionMenu: ProductSelectionMenuElement,
  };
}

export class ProductSelectorElement extends CrLitElement {
  static get is() {
    return 'product-selector';
  }

  static override get styles() {
    return getCss();
  }

  static override get properties() {
    return {
      selectedItem: {type: Object},
      excludedUrls: {type: Array},
    };
  }

  accessor selectedItem: UrlListEntry|null = null;
  accessor excludedUrls: string[] = [];

  override render() {
    return getHtml.bind(this)();
  }

  closeMenu() {
    this.$.productSelectionMenu.close();
  }

  protected showMenu_() {
    this.$.productSelectionMenu.showAt(this.$.currentProductContainer);
    this.$.currentProductContainer.classList.add('showing-menu');
  }

  protected onCloseMenu_() {
    this.$.currentProductContainer.classList.remove('showing-menu');
  }

  protected onCurrentProductContainerKeyDown_(e: KeyboardEvent) {
    if (e.key === ' ' || e.key === 'Enter') {
      e.preventDefault();
      e.stopPropagation();
      this.showMenu_();
    }
  }

  protected getUrl_(item: UrlListEntry) {
    return getAbbreviatedUrl(item.url);
  }

  protected getSelectedUrl_() {
    return this.selectedItem?.url ?? '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'product-selector': ProductSelectorElement;
  }
}

customElements.define(ProductSelectorElement.is, ProductSelectorElement);
