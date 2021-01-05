// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../img.js';
import '../module_header.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.m.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ModuleDescriptor} from '../module_descriptor.js';
import {ChromeCartProxy} from './chrome_cart_proxy.js';

/**
 * @fileoverview Implements the UI of chrome cart module. This module
 * shows pending carts for users on merchant sites so that users can
 * resume shopping journey.
 */

class ChromeCartModuleElement extends PolymerElement {
  static get is() {
    return 'ntp-chrome-cart-module';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!Array<!chromeCart.mojom.MerchantCart>} */
      cartItems: Array,
    };
  }

  /**
   * @param {string} url
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

  /**
   * @param {!Array<string>} imageUrls
   * @return {!Array<string>}
   * @private
   */
  getImagesToShow_(imageUrls) {
    return imageUrls.slice(0, 3);
  }

  /** @private */
  onDismissButtonClick_() {
    ChromeCartProxy.getInstance().handler.dismissCartModule();
    this.dispatchEvent(new CustomEvent('dismiss-module', {
      bubbles: true,
      composed: true,
      detail: {
        message: 'Your carts',
        restoreCallback: this.onRestore_.bind(this),
      },
    }));
  }

  /** @private */
  onRestore_() {
    ChromeCartProxy.getInstance().handler.restoreCartModule();
  }

  /**
   * @param {!Event} e
   * @private
   */
  onMenuButtonClick_(e) {
    e.preventDefault();
    const index = this.$.cartItemRepeat.indexForElement(
        e.target.parentElement.parentElement);
    this.$.actionMenu.showAt(e.target);
  }
}

customElements.define(ChromeCartModuleElement.is, ChromeCartModuleElement);

/** @return {!Promise<?HTMLElement>} */
async function createCartElement() {
  const {carts} =
      await ChromeCartProxy.getInstance().handler.getMerchantCarts();
  if (carts.length === 0) {
    return null;
  }
  const element = new ChromeCartModuleElement();
  element.cartItems = carts;
  return element;
}

/** @type {!ModuleDescriptor} */
export const chromeCartDescriptor = new ModuleDescriptor(
    /*id=*/ 'chrome_cart',
    /*heightPx=*/ 230, createCartElement);
