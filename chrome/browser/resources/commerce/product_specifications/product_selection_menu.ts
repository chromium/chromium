// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import './strings.m.js';

import type {BrowserProxy} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import {BrowserProxyImpl} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import type {UrlInfo} from 'chrome://resources/cr_components/commerce/shopping_service.mojom-webui.js';
import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
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

interface MenuSection {
  title: string;
  entries: UrlListEntry[];
  expanded: boolean;
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

      sections: Array,
    };
  }

  private shoppingApi_: BrowserProxy = BrowserProxyImpl.getInstance();

  selectedUrl: string;
  sections: MenuSection[];

  async showAt(element: HTMLElement) {
    const openUrlInfos = await this.shoppingApi_.getUrlInfosForOpenTabs();
    // Filter out URLs that match the selected item.
    const filteredOpenUrlInfos = openUrlInfos.urlInfos.filter(
        (urlInfo) => urlInfo.url.url !== this.selectedUrl);
    const openTabs = this.urlInfosToListEntries_(filteredOpenUrlInfos);

    const recentlyViewedUrlInfos =
        await this.shoppingApi_.getUrlInfosForRecentlyViewedTabs();
    const recentlyViewedTabs =
        this.urlInfosToListEntries_(recentlyViewedUrlInfos.urlInfos);

    const updatedSections: MenuSection[] = [];
    if (openTabs.length > 0) {
      updatedSections.push({
        title: loadTimeData.getString('openTabs'),
        entries: openTabs,
        expanded: true,
      });
    }
    if (recentlyViewedTabs.length > 0) {
      updatedSections.push({
        title: loadTimeData.getString('recentlyViewedTabs'),
        entries: recentlyViewedTabs,
        expanded: true,
      });
    }
    // Notify elements that use the |sections| property of its new value.
    this.set('sections', updatedSections);

    const rect = element.getBoundingClientRect();
    this.$.menu.get().showAt(element, {
      anchorAlignmentX: AnchorAlignment.AFTER_START,
      top: rect.bottom,
      left: rect.left,
    });
  }

  close() {
    this.$.menu.get().close();
  }

  private urlInfosToListEntries_(urlInfos: UrlInfo[]) {
    return urlInfos.map(({title, url}) => ({
                          title: title,
                          url: url.url,
                          imageUrl: url.url,
                        }));
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
