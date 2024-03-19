// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import type {BrowserProxy} from '//resources/cr_components/commerce/browser_proxy.js';
import {BrowserProxyImpl} from '//resources/cr_components/commerce/browser_proxy.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';

export class ProductSpecificationsElement extends PolymerElement {
  static get is() {
    return 'product-specifications-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      message_: {
        type: String,
        value: () => loadTimeData.getString('message'),
      },
    };
  }

  private shoppingApi_: BrowserProxy = BrowserProxyImpl.getInstance();
  private status_: string;

  constructor() {
    super();
    ColorChangeUpdater.forDocument().start();
  }

  override async connectedCallback() {
    super.connectedCallback();
    const params = new URLSearchParams(window.location.search);
    const urlsParam = params.get('urls');
    if (!urlsParam) {
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
    this.status_ =
        `Found: ${urls.length}. Resolved: ${productSpecs.products.length}`;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'compare-app': ProductSpecificationsElement;
  }
}

customElements.define(
    ProductSpecificationsElement.is, ProductSpecificationsElement);
