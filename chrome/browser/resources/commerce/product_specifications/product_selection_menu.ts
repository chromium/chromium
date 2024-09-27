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
import './images/icons.html.js';
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

export enum SectionType {
  NONE = 0,
  SUGGESTED = 1,
  RECENT = 2,
}

interface MenuSection {
  title: string;
  entries: UrlListEntry[];
  expanded: boolean;
  sectionType: SectionType;
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

      excludedUrls: {
        type: Array,
        value: () => [],
      },

      forNewColumn: {
        type: Boolean,
        value: false,
      },

      isTableFull: {
        type: Boolean,
        value: false,
      },

      sections: Array,
    };
  }

  private shoppingApi_: BrowserProxy = BrowserProxyImpl.getInstance();

  selectedUrl: string;
  excludedUrls: string[];
  forNewColumn: boolean;
  isTableFull: boolean;
  sections: MenuSection[];

  async showAt(element: HTMLElement) {
    const suggestedUrlInfos =
        await this.shoppingApi_.getUrlInfosForProductTabs();
    const filteredSuggestedUrlInfos =
        this.filterUrlInfos_(suggestedUrlInfos.urlInfos);
    const suggestedTabs =
        this.urlInfosToListEntries_(filteredSuggestedUrlInfos);

    const recentlyViewedUrlInfos =
        await this.shoppingApi_.getUrlInfosForRecentlyViewedTabs();
    const filteredRecentlyViewedUrlInfos =
        this.filterUrlInfos_(recentlyViewedUrlInfos.urlInfos);
    const recentlyViewedTabs =
        this.urlInfosToListEntries_(filteredRecentlyViewedUrlInfos);

    const updatedSections: MenuSection[] = [];
    if (suggestedTabs.length > 0) {
      updatedSections.push({
        title: loadTimeData.getString('suggestedTabs'),
        entries: suggestedTabs,
        expanded: true,
        sectionType: SectionType.SUGGESTED,
      });
    }
    if (recentlyViewedTabs.length > 0) {
      updatedSections.push({
        title: loadTimeData.getString('recentlyViewedTabs'),
        entries: recentlyViewedTabs,
        expanded: true,
        sectionType: SectionType.RECENT,
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

  // Filter out URLs that match the selected item or any excluded urls.
  private filterUrlInfos_(urlInfos: UrlInfo[]) {
    return urlInfos.filter(
        (urlInfo) => urlInfo.url.url !== this.selectedUrl &&
            !this.excludedUrls.includes(urlInfo.url.url));
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
        urlSection: (e.currentTarget as any).dataUrlSection || SectionType.NONE,
      },
    }));
  }

  private onRemoveClick_() {
    this.close();
    this.dispatchEvent(new CustomEvent('remove-url', {
      bubbles: true,
      composed: true,
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

  private showEmptySuggestionsMessage_(
      sections: MenuSection[], forNewColumn: boolean,
      isTableFull: boolean): boolean {
    return (!sections || sections.length === 0) &&
        !this.showTableFullMessage_(forNewColumn, isTableFull);
  }

  private showTableFullMessage_(forNewColumn: boolean, isTableFull: boolean):
      boolean {
    return forNewColumn && isTableFull;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'product-selection-menu': ProductSelectionMenuElement;
  }
}

customElements.define(
    ProductSelectionMenuElement.is, ProductSelectionMenuElement);
