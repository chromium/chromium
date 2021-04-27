// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../img.js';
import '../module_header.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.m.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {$$} from '../../utils.js';
import {ModuleDescriptor} from '../module_descriptor.js';

import {ChromeCartProxy} from './chrome_cart_proxy.js';

/**
 * Implements the UI of chrome cart module. This module shows pending carts for
 * users on merchant sites so that users can resume shopping journey.
 * @polymer
 * @extends {PolymerElement}
 */
class ChromeCartModuleElement extends mixinBehaviors
([I18nBehavior], PolymerElement) {
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

      /** @type {string} */
      headerChipText: String,

      /** @type {string} */
      headerDescriptionText: {
        type: String,
        reflectToAttribute: true,
      },

      /** @type {boolean} */
      showDiscountConsent: Boolean,

      /** @private {boolean} */
      showLeftScrollButton_: Boolean,

      /** @private {boolean} */
      showRightScrollButton_: Boolean,

      /** @private {string} */
      cartMenuHideItem_: String,

      /** @private {string} */
      cartMenuRemoveItem_: String,

      /**
       * Data about the most recently dismissed cart item.
       * @type {?{message: string, restoreCallback: function()}}
       * @private
       */
      dismissedCartData_: {
        type: Object,
        value: null,
      },

      /** @private {string} */
      confirmDiscountConsentString_: String,

      /** @private {string} */
      discountConsentIconSrc_: String,
    };
  }

  constructor() {
    super();

    /** @private {IntersectionObserver} */
    this.intersectionObserver_ = null;

    /** @type {string} */
    this.scrollBehavior = 'smooth';

    /** @private {number} */
    this.currentMenuIndex_ = 0;
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
    this.currentMenuIndex_ =
        this.$.cartItemRepeat.indexForElement(e.target.parentElement);
    const merchant = this.cartItems[this.currentMenuIndex_].merchant;
    this.cartMenuHideItem_ =
        loadTimeData.getStringF('modulesCartCartMenuHideMerchant', merchant);
    this.cartMenuRemoveItem_ =
        loadTimeData.getStringF('modulesCartCartMenuRemoveMerchant', merchant);
    this.$.cartActionMenu.showAt(e.target);
  }

  /** @private */
  async onCartHide_() {
    this.$.cartActionMenu.close();
    const merchant = this.cartItems[this.currentMenuIndex_].merchant;
    const cartUrl = this.cartItems[this.currentMenuIndex_].cartUrl;

    await ChromeCartProxy.getInstance().handler.hideCart(cartUrl);
    this.resetCartData_();

    this.dismissedCartData_ = {
      message: loadTimeData.getStringF(
          'modulesCartCartMenuHideMerchantToastMessage', merchant),
      restoreCallback: async () => {
        await ChromeCartProxy.getInstance().handler.restoreHiddenCart(cartUrl);
      },
    };
    $$(this, '#dismissCartToast').show();
  }

  /** @private */
  async onCartRemove_() {
    this.$.cartActionMenu.close();
    const merchant = this.cartItems[this.currentMenuIndex_].merchant;
    const cartUrl = this.cartItems[this.currentMenuIndex_].cartUrl;

    await ChromeCartProxy.getInstance().handler.removeCart(cartUrl);
    this.resetCartData_();

    this.dismissedCartData_ = {
      message: loadTimeData.getStringF(
          'modulesCartCartMenuRemoveMerchantToastMessage', merchant),
      restoreCallback: async () => {
        await ChromeCartProxy.getInstance().handler.restoreRemovedCart(cartUrl);
      },
    };
    $$(this, '#dismissCartToast').show();
  }

  /** @private */
  async onUndoDismissCartButtonClick_() {
    // Restore the module item.
    await this.dismissedCartData_.restoreCallback();
    this.resetCartData_();

    // Notify the user.
    $$(this, '#dismissCartToast').hide();

    this.dismissedCartData_ = null;
  }

  /** @private */
  async resetCartData_() {
    // TODO(crbug.com/1157892): Hide the module silently if there is no cart
    // item to show.
    const {carts} =
        await ChromeCartProxy.getInstance().handler.getMerchantCarts();
    this.cartItems = carts;
  }

  /** @private */
  onDismissButtonClick_() {
    ChromeCartProxy.getInstance().handler.hideCartModule();
    this.dispatchEvent(new CustomEvent('dismiss-module', {
      bubbles: true,
      composed: true,
      detail: {
        message:
            loadTimeData.getString('modulesCartModuleMenuHideToastMessage'),
        restoreCallback: () => {
          ChromeCartProxy.getInstance().handler.restoreHiddenCartModule();
          chrome.metricsPrivate.recordUserAction(
              'NewTabPage.Carts.UndoHideModule');
        },
      },
    }));
    chrome.metricsPrivate.recordUserAction('NewTabPage.Carts.HideModule');
  }

  /** @private */
  onDisableButtonClick_() {
    this.dispatchEvent(new CustomEvent('disable-module', {
      bubbles: true,
      composed: true,
      detail: {
        message: loadTimeData.getStringF(
            'disableModuleToastMessage',
            loadTimeData.getString('modulesCartLowerYour')),
        restoreCallback: () => {
          chrome.metricsPrivate.recordUserAction(
              'NewTabPage.Carts.UndoRemoveModule');
        },
      },
    }));
    chrome.metricsPrivate.recordUserAction('NewTabPage.Carts.RemoveModule');
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
    chrome.metricsPrivate.recordUserAction('NewTabPage.Carts.RightScrollClick');
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
    chrome.metricsPrivate.recordUserAction('NewTabPage.Carts.LeftScrollClick');
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
    let leftPosition = carts[index].offsetLeft - scrollOffset;
    // TODO(crbug.com/1198632): This could make a left scroll jump over cart
    // items.
    if (index === 0) {
      const consentCard = this.shadowRoot.getElementById('consentCard');
      if (consentCard) {
        leftPosition -= consentCard.offsetWidth;
      }
    }
    this.$.cartCarousel.scrollTo({
      top: 0,
      left: leftPosition,
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

  /**
   * @param {!Event} e
   * @private
   */
  onCartItemClick_(e) {
    const index = this.$.cartItemRepeat.indexForElement(e.target);
    ChromeCartProxy.getInstance().handler.onCartItemClicked(index);
    this.dispatchEvent(new Event('usage', {bubbles: true, composed: true}));
  }

  /** @private */
  onDisallowDiscount_() {
    this.showDiscountConsent = false;
    this.confirmDiscountConsentString_ =
        loadTimeData.getString('modulesCartDiscountConsentRejectConfirmation');
    $$(this, '#confirmDiscountConsentToast').show();
    ChromeCartProxy.getInstance().handler.onDiscountConsentAcknowledged(false);
  }

  /** @private */
  onAllowDiscount_() {
    this.showDiscountConsent = false;
    this.confirmDiscountConsentString_ =
        loadTimeData.getString('modulesCartDiscountConsentAcceptConfirmation');
    $$(this, '#confirmDiscountConsentToast').show();
    ChromeCartProxy.getInstance().handler.onDiscountConsentAcknowledged(true);
  }

  /** @private */
  onConfirmDiscountConsentClick_() {
    $$(this, '#confirmDiscountConsentToast').hide();
  }
}

customElements.define(ChromeCartModuleElement.is, ChromeCartModuleElement);

/** @return {!Promise<?HTMLElement>} */
async function createCartElement() {
  const {welcomeVisible} =
      await ChromeCartProxy.getInstance().handler.getWarmWelcomeVisible();
  const {carts} =
      await ChromeCartProxy.getInstance().handler.getMerchantCarts();
  const {consentVisible} = await ChromeCartProxy.getInstance()
                               .handler.getDiscountConsentCardVisible();
  ChromeCartProxy.getInstance().handler.onModuleCreated(carts.length);
  if (carts.length === 0) {
    return null;
  }
  const element = new ChromeCartModuleElement();
  if (welcomeVisible) {
    element.headerChipText = loadTimeData.getString('modulesCartHeaderNew');
    element.headerDescriptionText =
        loadTimeData.getString('modulesCartWarmWelcome');
  }
  element.cartItems = carts;
  element.showDiscountConsent = consentVisible;
  return element;
}

/** @type {!ModuleDescriptor} */
export const chromeCartDescriptor = new ModuleDescriptor(
    /*id=*/ 'chrome_cart',
    /*name=*/ loadTimeData.getString('modulesCartSentence'), createCartElement);
