// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';
import './product_selector.js';
import './table.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import type {BrowserProxy} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import {BrowserProxyImpl} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {Router} from './router.js';
import type {TableColumn, TableRow} from './table.js';

export class ProductSpecificationsElement extends PolymerElement {
  static get is() {
    return 'product-specifications-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      showEmptyState_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      specsTable_: {
        type: Object,
        value: {},
      },
    };
  }

  private showEmptyState_: boolean;
  private specsTable_: {columns: TableColumn[], rows: TableRow[]};

  private shoppingApi_: BrowserProxy = BrowserProxyImpl.getInstance();

  constructor() {
    super();
    ColorChangeUpdater.forDocument().start();
  }

  override async connectedCallback() {
    super.connectedCallback();
    const router = Router.getInstance();
    const params = new URLSearchParams(router.getCurrentQuery());
    const urlsParam = params.get('urls');
    if (!urlsParam) {
      this.showEmptyState_ = true;
      return;
    }

    let urls: string[] = [];
    try {
      urls = JSON.parse(urlsParam);
    } catch (_) {
      return;
    }

    const {productSpecs} =
        await this.shoppingApi_.getProductSpecificationsForUrls(
            urls.map(url => {
              return {url};
            }));

    const rows: TableRow[] = [];
    productSpecs.productDimensionMap.forEach((value: string, key: bigint) => {
      rows.push({
        title: value,
        values: productSpecs.products.map(
            p => p.productDimensionValues.get(key)?.join(',') ?? ''),
      });
    });

    this.specsTable_ = {
      columns: productSpecs.products.map(p => {
        return {
          selectedItem: {
            title: p.title,
            // TODO(b/335637140): Replace with actual URL once available
            url: 'https://placeholder.com',
            imageUrl: p.imageUrl.url,
          },
        };
      }),
      rows,
    };
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'product-specifications-app': ProductSpecificationsElement;
  }
}

customElements.define(
    ProductSpecificationsElement.is, ProductSpecificationsElement);
