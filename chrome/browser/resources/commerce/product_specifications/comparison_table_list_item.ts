// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';

import type {CrUrlListItemElement} from '//resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import {type PluralStringProxy, PluralStringProxyImpl} from '//resources/js/plural_string_proxy.js';
import type {Uuid} from '//resources/mojo/mojo/public/mojom/base/uuid.mojom-webui.js';
import {type ProductSpecificationsBrowserProxy, ProductSpecificationsBrowserProxyImpl} from 'chrome://resources/cr_components/commerce/product_specifications_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement, type PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {getCss} from './comparison_table_list_item.css.js';
import {getHtml} from './comparison_table_list_item.html.js';

export type ComparisonTableListItemClickEvent = CustomEvent<{uuid: Uuid}>;

export interface ComparisonTableListItemElement {
  $: {
    item: CrUrlListItemElement,
    numItems: HTMLDivElement,
  };
}

export class ComparisonTableListItemElement extends CrLitElement {
  static get is() {
    return 'comparison-table-list-item';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      name: {type: String},
      uuid: {type: Object},
      numUrls: {type: Number},
      imageUrl: {type: Object},
      tableUrl_: {type: Object},
      numItemsString_: {type: String},
    };
  }

  name: string = '';
  uuid: Uuid = {value: ''};
  numUrls: number = 0;
  imageUrl: Url|null = null;

  protected tableUrl_: Url = {url: ''};
  protected numItemsString_: string = '';
  private productSpecificationsProxy_: ProductSpecificationsBrowserProxy =
      ProductSpecificationsBrowserProxyImpl.getInstance();
  private pluralStringProxy_: PluralStringProxy =
      PluralStringProxyImpl.getInstance();

  override async connectedCallback() {
    super.connectedCallback();

    const {url} =
        await this.productSpecificationsProxy_.getComparisonTableUrlForUuid(
            this.uuid);
    this.tableUrl_ = url;

    this.updateNumItemsString_();
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('numUrls')) {
      this.updateNumItemsString_();
    }
  }

  protected getTitle_() {
    return loadTimeData.getStringF('tableListItemTitle', this.name);
  }

  protected async updateNumItemsString_() {
    this.numItemsString_ =
        await this.pluralStringProxy_.getPluralString('numItems', this.numUrls);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'comparison-table-list-item': ComparisonTableListItemElement;
  }
}

customElements.define(
    ComparisonTableListItemElement.is, ComparisonTableListItemElement);
