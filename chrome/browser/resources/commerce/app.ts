// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import type {BrowserProxy} from '//resources/cr_components/commerce/browser_proxy.js';
import {BrowserProxyImpl} from '//resources/cr_components/commerce/browser_proxy.js';
import type {BookmarkProductInfo} from '//resources/cr_components/commerce/shopping_service.mojom-webui.js';
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
  private lastSubscribedId: string = 'N/A';

  constructor() {
    super();
    ColorChangeUpdater.forDocument().start();
  }

  override connectedCallback() {
    super.connectedCallback();

    const callbackRouter = this.shoppingApi_.getCallbackRouter();
    callbackRouter.priceTrackedForBookmark.addListener(
        (product: BookmarkProductInfo) => {
          this.lastSubscribedId = product.info.clusterId.toString();
        });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'compare-app': ProductSpecificationsElement;
  }
}

customElements.define(
    ProductSpecificationsElement.is, ProductSpecificationsElement);
