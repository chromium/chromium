// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import './images/icons.html.js';
import '/strings.m.js';

import type {UrlInfo} from 'chrome://resources/cr_components/commerce/shopping_service.mojom-webui.js';
import type {ShoppingServiceBrowserProxy} from 'chrome://resources/cr_components/commerce/shopping_service_browser_proxy.js';
import {ShoppingServiceBrowserProxyImpl} from 'chrome://resources/cr_components/commerce/shopping_service_browser_proxy.js';
import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderLitElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './product_selection_menu.css.js';
import {getHtml} from './product_selection_menu.html.js';
import {getAbbreviatedUrl} from './utils.js';
import type {UrlListEntry} from './utils.js';

export interface ProductSelectionMenuElement {
  $: {
    menu: CrLazyRenderLitElement<CrActionMenuElement>,
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

export class ProductSelectionMenuElement extends CrLitElement {
  static get is() {
    return 'product-selection-menu';
  }

  static override get styles() {
    return getCss();
  }

  static override get properties() {
    return {
      selectedUrl: {type: String},
      excludedUrls: {type: Array},
      forNewColumn: {type: Boolean},
      isTableFull: {type: Boolean},
      sections: {type: Array},
    };
  }

  private shoppingApi_: ShoppingServiceBrowserProxy =
      ShoppingServiceBrowserProxyImpl.getInstance();

  accessor selectedUrl: string = '';
  accessor excludedUrls: string[] = [];
  accessor forNewColumn: boolean = false;
  accessor isTableFull: boolean = false;
  accessor sections: MenuSection[] = [];

  override render() {
    return getHtml.bind(this)();
  }

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
    this.sections = updatedSections;

    const rect = element.getBoundingClientRect();
    this.$.menu.get().showAt(element, {
      anchorAlignmentX: AnchorAlignment.AFTER_START,
      top: rect.bottom,
      left: rect.left,
    });
  }

  close() {
    const menu = this.$.menu.getIfExists();
    if (menu) {
      menu.close();
    }
  }

  protected expandedChanged_(
      e: CustomEvent<{value: boolean}>, section: MenuSection) {
    section.expanded = e.detail.value;

    // Manually request an update since the variable controlling the expansion
    // state is not a top-level object.
    this.requestUpdate();
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

  protected onSelect_(e: Event) {
    const currentTarget = e.currentTarget as HTMLElement;
    const itemIndex = Number(currentTarget.dataset['itemIndex']);
    const sectionIndex = Number(currentTarget.dataset['sectionIndex']);
    const sectionType =
        Number(currentTarget.dataset['sectionType']) as SectionType;
    const item = this.sections[sectionIndex]?.entries[itemIndex] || null;
    assert(!!item);
    this.close();
    this.dispatchEvent(new CustomEvent('selected-url-change', {
      bubbles: true,
      composed: true,
      detail: {
        url: item.url,
        urlSection: sectionType,
      },
    }));
  }

  protected onRemoveClick_() {
    this.close();
    this.dispatchEvent(new CustomEvent('remove-url', {
      bubbles: true,
      composed: true,
    }));
  }

  protected onClose_() {
    this.dispatchEvent(new CustomEvent('close-menu', {
      bubbles: true,
      composed: true,
    }));
  }

  protected getUrl_(item: UrlListEntry) {
    return getAbbreviatedUrl(item.url);
  }

  protected showEmptySuggestionsMessage_(): boolean {
    return (!this.sections || this.sections.length === 0) &&
        !this.showTableFullMessage_();
  }

  protected showTableFullMessage_(): boolean {
    return this.forNewColumn && this.isTableFull;
  }

  protected isLastSection_(sectionIndex: number) {
    return this.sections && sectionIndex === this.sections.length - 1;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'product-selection-menu': ProductSelectionMenuElement;
  }
}

customElements.define(
    ProductSelectionMenuElement.is, ProductSelectionMenuElement);
