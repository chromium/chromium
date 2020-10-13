// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_grid/cr_grid.js';

import '../../img.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ModuleDescriptor} from '../module_descriptor.js';
import {ChromeCartProxy} from './chrome_cart_proxy.js';

/**
 * @fileoverview A dummy module, which serves as an example and a helper to
 * build out the NTP module framework.
 */

class DummyModuleElement extends PolymerElement {
  static get is() {
    return 'ntp-dummy-module';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!Array<!chromeCart.mojom.ChromeCartDataItem>} */
      tiles: Array,
    };
  }

  constructor() {
    super();
    this.initializeData_();
  }

  /** @private */
  async initializeData_() {
    const tileData = await ChromeCartProxy.getInstance().handler.getData();
    this.tiles = tileData.data;
  }

  /**
   * @return {string}
   * @private
   */
  getFaviconUrl_(url) {
    const faviconUrl = new URL('chrome://favicon2/');
    faviconUrl.searchParams.set('size', '24');
    faviconUrl.searchParams.set('scale_factor', '1x');
    faviconUrl.searchParams.set('show_fallback_monogram', '');
    faviconUrl.searchParams.set('page_url', url);
    return faviconUrl.href;
  }

  shouldShowThumbnailGrid_(length) {
    return length > 1;
  }

  shouldShowExtraItemCard_(length) {
    return length > 4;
  }

  getTextForExtraItemCard_(length) {
    return '+' + (length - 3);
  }

  getImagesToShow_(imageUrls) {
    if (imageUrls.length === 1) {
      return imageUrls[0];
    } else if (imageUrls.length > 4) {
      return imageUrls.slice(0, 3);
    } else {
      return imageUrls;
    }
  }
}

customElements.define(DummyModuleElement.is, DummyModuleElement);

/** @type {!ModuleDescriptor} */
export const dummyDescriptor = new ModuleDescriptor(
    /*id=*/ 'chromeCart', /*name=*/ loadTimeData.getString('modulesDummyName'),
    /*heightPx=*/ 180, () => Promise.resolve({
      element: new DummyModuleElement(),
      title: loadTimeData.getString('modulesDummyTitle'),
    }));
