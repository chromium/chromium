// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../module_header.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';

import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {I18nBehavior, loadTimeData} from '../../i18n_setup.js';
import {$$} from '../../utils.js';
import {ChromeCartProxy} from '../cart/chrome_cart_proxy.js';
import {ModuleDescriptor} from '../module_descriptor.js';

/**
 * Implements the UI of chrome cart module. This module shows pending carts for
 * users on merchant sites so that users can resume shopping journey.
 * @polymer
 * @extends {PolymerElement}
 */
class ChromeCartModuleElement extends mixinBehaviors
([I18nBehavior], PolymerElement) {
  static get is() {
    return 'ntp-modules-redesigned';
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
   * @param {!Array<string>} imageUrls
   * @return {!Array<string>}
   * @private
   */
  getImagesToShow_(imageUrls) {
    return imageUrls.slice(0, 3);
  }

  /**
   * @param {number} length
   * @return {boolean}
   * @private
   */
  isOne_(length) {
    return length === 1;
  }

  /**
   * @param {!Event} e
   * @private
   */
  onCartMenuButtonClick_(e) {
    e.preventDefault();
    e.stopPropagation();
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

    this.dismissedCartData_ = {
      message: loadTimeData.getStringF(
          'modulesCartCartMenuHideMerchantToastMessage', merchant),
      restoreCallback: async () => {
        await ChromeCartProxy.getInstance().handler.restoreHiddenCart(cartUrl);
      },
    };
    const isModuleVisible = await this.resetCartData_();
    if (isModuleVisible) {
      $$(this, '#dismissCartToast').show();
    }
  }

  /** @private */
  async onCartRemove_() {
    this.$.cartActionMenu.close();
    const merchant = this.cartItems[this.currentMenuIndex_].merchant;
    const cartUrl = this.cartItems[this.currentMenuIndex_].cartUrl;

    await ChromeCartProxy.getInstance().handler.removeCart(cartUrl);

    this.dismissedCartData_ = {
      message: loadTimeData.getStringF(
          'modulesCartCartMenuRemoveMerchantToastMessage', merchant),
      restoreCallback: async () => {
        await ChromeCartProxy.getInstance().handler.restoreRemovedCart(cartUrl);
      },
    };
    const isModuleVisible = await this.resetCartData_();
    if (isModuleVisible) {
      $$(this, '#dismissCartToast').show();
    }
  }

  /** @private */
  async onUndoDismissCartButtonClick_() {
    // Restore the module item.
    await this.dismissedCartData_.restoreCallback();
    this.dismissedCartData_ = null;
    this.resetCartData_();

    // Notify the user.
    $$(this, '#dismissCartToast').hide();
  }

  /**
   * @return {!Promise<!boolean>} Whether the module is visible after reset.
   * @private
   */
  async resetCartData_() {
    const {carts} =
        await ChromeCartProxy.getInstance().handler.getMerchantCarts();
    this.cartItems = carts;
    const isModuleVisible = this.cartItems.length !== 0;
    if (!isModuleVisible && this.dismissedCartData_ !== null) {
      this.dispatchEvent(new CustomEvent('dismiss-module', {
        bubbles: true,
        composed: true,
        detail: {
          message: this.dismissedCartData_.message,
          restoreCallback: async () => {
            chrome.metricsPrivate.recordUserAction(
                'NewTabPage.Carts.RestoreLastCartRestoresModule');
            await this.dismissedCartData_.restoreCallback();
            this.dismissedCartData_ = null;
            const {carts} =
                await ChromeCartProxy.getInstance().handler.getMerchantCarts();
            this.cartItems = carts;
          },
        },
      }));
      chrome.metricsPrivate.recordUserAction(
          'NewTabPage.Carts.DismissLastCartHidesModule');
    }
    return isModuleVisible;
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
    let firstVisibleIndex = 0;
    for (let i = carts.length - 1; i >= 0; i--) {
      if (this.getVisibilityForIndex_(i)) {
        firstVisibleIndex = i;
      }
    }
    this.scrollToIndex_(Math.max(0, firstVisibleIndex - 1));
    chrome.metricsPrivate.recordUserAction('NewTabPage.Carts.LeftScrollClick');
  }

  /**
   * @param {!number} index The target index to scroll to.
   * @private
   */
  scrollToIndex_(index) {
    const carts = this.$.cartCarousel.querySelectorAll('.cart-item');
    let leftPosition = carts[index].offsetLeft;
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
    return cart && (cart.offsetLeft === cartCarousel.scrollLeft) &&
        (cartCarousel.clientWidth <= cart.offsetWidth);
  }

  /**
   * @param {!Event} e
   * @private
   */
  async onCartItemClick_(e) {
    const index = this.$.cartItemRepeat.indexForElement(e.target);
    // When rule-based discount is enabled, clicking on the cart wouldn't
    // trigger navigation immediately. Instead, we'll fetch discount URL from
    // browser process and re-bind URL. Then, we create a new pointer event by
    // cloning the initial one so that we can re-trigger a navigation with the
    // new URL. This is to keep the navigation in render process for security
    // reasons.
    if (loadTimeData.getBoolean('ruleBasedDiscountEnabled') &&
        (e.shouldNavigate === undefined || e.shouldNavigate === false)) {
      e.preventDefault();
      const {discountUrl} =
          await ChromeCartProxy.getInstance().handler.getDiscountURL(
              this.cartItems[index].cartUrl);
      this.set(`cartItems.${index}.cartUrl`, discountUrl);
      const cloneEvent = new PointerEvent(e.type, e);
      cloneEvent.shouldNavigate = true;
      this.$.cartCarousel.querySelectorAll('.cart-item')[index].dispatchEvent(
          cloneEvent);
      return;
    }
    this.dispatchEvent(new Event('usage', {bubbles: true, composed: true}));
    chrome.metricsPrivate.recordSmallCount('NewTabPage.Carts.ClickCart', index);
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
  // getWarmWelcomeVisible makes server-side change and might flip the status of
  // whether welcome surface should show or not. Anything whose visibility
  // dependes on welcome surface (e.g. RBD consent) should check before
  // getWarmWelcomeVisible.
  const {consentVisible} = await ChromeCartProxy.getInstance()
                               .handler.getDiscountConsentCardVisible();
  const {welcomeVisible} =
      await ChromeCartProxy.getInstance().handler.getWarmWelcomeVisible();
  const {carts} =
      await ChromeCartProxy.getInstance().handler.getMerchantCarts();
  chrome.metricsPrivate.recordSmallCount(
      'NewTabPage.Carts.CartCount', carts.length);
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
