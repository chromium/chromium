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

import type {BrowserProxy} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import {BrowserProxyImpl} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './product_selection_menu.html.js';
import {getAbbreviatedUrl} from './utils.js';
import type {UrlListEntry} from './utils.js';

export interface ProductSelectionMenuElement {
  $: {
    menu: CrLazyRenderElement<CrActionMenuElement>,
  };
}

export class ProductSelectionMenuElement extends PolymerElement {
  static get is() {
    return 'product-selection-menu';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      selectedUrl: {
        type: String,
        value: '',
      },

      openTabs: {
        type: Array,
        value: () => [],
      },

      openTabsExpanded_: {
        type: Boolean,
        value: true,
      },
    };
  }

  private shoppingApi_: BrowserProxy = BrowserProxyImpl.getInstance();

  selectedUrl: string;
  openTabs: UrlListEntry[];

  private openTabsExpanded_: boolean;

  async showAt(element: HTMLElement) {
    const {urlInfos} = await this.shoppingApi_.getUrlInfosForOpenTabs();
    // Filter out URLs that match the selected item.
    const filteredUrlInfos =
        urlInfos.filter((urlInfo) => urlInfo.url.url !== this.selectedUrl);
    this.openTabs = filteredUrlInfos.map(({title, url}) => ({
                                           title: title,
                                           url: url.url,
                                           imageUrl: url.url,
                                         }));

    const rect = element.getBoundingClientRect();
    this.$.menu.get().showAt(element, {
      anchorAlignmentX: AnchorAlignment.CENTER,
      top: rect.bottom,
      left: rect.left,
    });
  }

  close() {
    this.$.menu.get().close();
  }

  private onSelect_(e: DomRepeatEvent<UrlListEntry>) {
    this.close();
    this.dispatchEvent(new CustomEvent('selected-url-change', {
      bubbles: true,
      composed: true,
      detail: {
        url: e.model.item.url,
      },
    }));
  }

  private onClose_() {
    this.dispatchEvent(new CustomEvent('close-menu', {
      bubbles: true,
      composed: true,
    }));
  }

  private getUrl_(item: UrlListEntry) {
    return getAbbreviatedUrl(item.url);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'product-selection-menu': ProductSelectionMenuElement;
  }
}

customElements.define(
    ProductSelectionMenuElement.is, ProductSelectionMenuElement);
