// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../img.js';
import '../module_header.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
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

      /** @private {boolean} */
      showLeftScrollButton_: Boolean,

      /** @private {boolean} */
      showRightScrollButton_: Boolean,
    };
  }

  constructor() {
    super();

    /** @private {IntersectionObserver} */
    this.intersectionObserver_ = null;

    /** @type {string} */
    this.scrollBehavior = 'smooth';
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    const leftProbe = this.$.cartCarousel.querySelector('#leftProbe');
    const rightProbe = this.$.cartCarousel.querySelector('#rightProbe');
    this.intersectionObserver_ = new IntersectionObserver(entries => {
      entries.forEach(({target, intersectionRatio}) => {
        const show = intersectionRatio === 0;
        if (target === leftProbe) {
          this.showLeftScrollButton_ = show;
          if (show) {
            this.dispatchEvent(new Event('left-scroll-show'));
          } else {
            this.dispatchEvent(new Event('left-scroll-hide'));
          }
        } else if (target === rightProbe) {
          this.showRightScrollButton_ = show;
          if (show) {
            this.dispatchEvent(new Event('right-scroll-show'));
          } else {
            this.dispatchEvent(new Event('right-scroll-hide'));
          }
        }
      });
    }, {root: this.$.cartCarousel});
    this.shadowRoot.querySelectorAll('.probe').forEach(
        el => this.intersectionObserver_.observe(el));
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    this.intersectionObserver_.disconnect();
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

  /**
   * @param {!Event} e
   * @private
   */
  onCartMenuButtonClick_(e) {
    e.preventDefault();
    this.$.cartActionMenu.showAt(e.target);
  }

  /**
   * @param {!Event} e
   * @private
   */
  onModuleMenuButtonClick_(e) {
    e.preventDefault();
    this.$.moduleActionMenu.showAt(e.target);
  }

  /** @private */
  onModuleHide_() {
    ChromeCartProxy.getInstance().handler.hideCartModule();
    this.dispatchEvent(new CustomEvent('dismiss-module', {
      bubbles: true,
      composed: true,
      detail: {
        message:
            loadTimeData.getString('modulesCartModuleMenuHideToastMessage'),
        restoreCallback: () => {
          ChromeCartProxy.getInstance().handler.restoreHiddenCartModule();
        },
      },
    }));
  }

  /** @private */
  onModuleRemove_() {
    ChromeCartProxy.getInstance().handler.removeCartModule();
    this.dispatchEvent(new CustomEvent('dismiss-module', {
      bubbles: true,
      composed: true,
      detail: {
        message:
            loadTimeData.getString('modulesCartModuleMenuRemoveToastMessage'),
        restoreCallback: () => {
          ChromeCartProxy.getInstance().handler.restoreRemovedCartModule();
        },
      },
    }));
  }

  /**
   * Gets called when the right scroll button is clicked to show the next items
   * on the right.
   * @private
   */
  onRightScrollClick_() {
    const carts = this.$.cartCarousel.querySelectorAll('.cart-item');
    let lastVisibleIndex = 0;
    for (let i = 0; i < carts.length; i++) {
      if (this.getVisibilityForIndex_(i)) {
        lastVisibleIndex = i;
      }
    }
    this.scrollToIndex_(lastVisibleIndex + 1);
  }

  /**
   * Gets called when the left scroll button is clicked to show the previous
   * items on the left.
   * @private
   */
  onLeftScrollClick_() {
    const carts = this.$.cartCarousel.querySelectorAll('.cart-item');
    let visibleRange = 0, firstVisibleIndex = 0;
    for (let i = carts.length - 1; i >= 0; i--) {
      if (this.getVisibilityForIndex_(i)) {
        visibleRange += 1;
        firstVisibleIndex = i;
      }
    }
    this.scrollToIndex_(Math.max(0, firstVisibleIndex - visibleRange));
  }

  /**
   * @param {!number} index The target index to scroll to.
   * @private
   */
  scrollToIndex_(index) {
    const carts = this.$.cartCarousel.querySelectorAll('.cart-item');
    // Calculate scroll shadow width as scroll offset.
    const leftScrollShadow = this.shadowRoot.getElementById('leftScrollShadow');
    const rightScrollShadow =
        this.shadowRoot.getElementById('rightScrollShadow');
    const scrollOffset = Math.max(
        leftScrollShadow ? leftScrollShadow.offsetWidth : 0,
        rightScrollShadow ? rightScrollShadow.offsetWidth : 0);
    this.$.cartCarousel.scrollTo({
      top: 0,
      left: carts[index].offsetLeft - scrollOffset,
      behavior: this.scrollBehavior,
    });
  }

  /**
   * @param {!number} index
   * @return {!boolean} True if the item at index is completely visible.
   * @private
   */
  getVisibilityForIndex_(index) {
    const cartCarousel = this.$.cartCarousel;
    const cart = cartCarousel.querySelectorAll('.cart-item')[index];
    return cart && (cart.offsetLeft > cartCarousel.scrollLeft) &&
        (cartCarousel.scrollLeft + cartCarousel.clientWidth) >
        (cart.offsetLeft + cart.offsetWidth);
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
