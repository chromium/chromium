// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';

import type {BrowserProxy} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import {BrowserProxyImpl} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrExpandButtonElement} from 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import type {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import type {CrUrlListItemElement} from 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './product_selector.html.js';

export interface UrlListEntry {
  title: string;
  url: string;
  imageUrl: string;
}

export interface ProductSelectorElement {
  $: {
    currentItemButton: CrExpandButtonElement,
    currentProduct: CrUrlListItemElement,
    currentProductContainer: HTMLElement,
    productSelectionMenu: CrLazyRenderElement<CrActionMenuElement>,
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
      openTabs: {
        type: Array,
        value: () => [],
      },
      openTabsExpanded: {
        type: Boolean,
        value: true,
      },
    };
  }

  private shoppingApi_: BrowserProxy = BrowserProxyImpl.getInstance();

  selectedItem: UrlListEntry;
  openTabs: UrlListEntry[];

  private openTabsExpanded: boolean;

  private async onShowMenu() {
    const {urlInfos} = await this.shoppingApi_.getUrlInfosForOpenTabs();
    this.openTabs = urlInfos.map(({title, url}) => ({
                                   title: title,
                                   url: url.url,
                                   imageUrl: url.url,
                                 }));

    const rect = this.$.currentProductContainer.getBoundingClientRect();
    this.$.productSelectionMenu.get().showAt(this.$.currentProductContainer, {
      anchorAlignmentX: AnchorAlignment.CENTER,
      top: rect.bottom,
      left: rect.left,
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'product-selector': ProductSelectorElement;
  }
}

customElements.define(ProductSelectorElement.is, ProductSelectorElement);
