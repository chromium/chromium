// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../module_header.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';

import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {DomIf, DomRepeat, DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MerchantCart} from '../../chrome_cart.mojom-webui.js';
import {I18nMixin, loadTimeData} from '../../i18n_setup.js';
import {recordOccurence} from '../../metrics_utils.js';
import {InfoDialogElement} from '../info_dialog.js';
import {ModuleDescriptor} from '../module_descriptor.js';

import {ChromeCartProxy} from './chrome_cart_proxy.js';
import {DiscountConsentVariation} from './discount_consent_card.js';
import {getTemplate} from './module.html.js';

export interface ChromeCartModuleElement {
  $: {
    cartActionMenu: CrActionMenuElement,
    cartCarousel: HTMLElement,
    cartItemRepeat: DomRepeat,
    confirmDiscountConsentButton: HTMLElement,
    confirmDiscountConsentMessage: HTMLElement,
    confirmDiscountConsentToast: CrToastElement,
    consentCardElement: DomIf,
    dismissCartToast: CrToastElement,
    dismissCartToastMessage: HTMLElement,
    hideCartButton: HTMLElement,
    infoDialogRender: CrLazyRenderElement<InfoDialogElement>,
    removeCartButton: HTMLElement,
    undoDismissCartButton: HTMLElement,
  };
}

/**
 * Implements the UI of chrome cart module. This module shows pending carts for
 * users on merchant sites so that users can resume shopping journey.
 */
export class ChromeCartModuleElement extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'ntp-chrome-cart-module';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      cartItems: Array,
      headerChipText: String,

      headerDescriptionText: {
        type: String,
        reflectToAttribute: true,
      },

      showDiscountConsent: Boolean,
      showLeftScrollButton_: Boolean,
      showRightScrollButton_: Boolean,
      cartMenuHideItem_: String,
      cartMenuRemoveItem_: String,

      /** Data about the most recently dismissed cart item. */
      dismissedCartData_: {
        type: Object,
        value: null,
      },

      confirmDiscountConsentString_: String,
      discountConsentHasTwoSteps_: {
        type: Boolean,
        value: () =>
            loadTimeData.getInteger('modulesCartDiscountConsentVariation') >
            DiscountConsentVariation.StringChange
      },
      firstThreeCartItems_:
          {type: Array, computed: 'computeFirstThreeCartItems_(cartItems)'}
    };
  }

  cartItems: MerchantCart[];
  headerChipText: string;
  headerDescriptionText: string;
  showDiscountConsent: boolean;
  scrollBehavior: ScrollBehavior = 'smooth';
  private showLeftScrollButton_: boolean;
  private showRightScrollButton_: boolean;
  private cartMenuHideItem_: string;
  private cartMenuRemoveItem_: string;
  private dismissedCartData_: {message: string, restoreCallback: () => void}|
      null;
  private confirmDiscountConsentString_: string;

  private intersectionObserver_: IntersectionObserver|null = null;
  private currentMenuIndex_: number = 0;
  private discountConsentHasTwoSteps_: boolean;
  private firstThreeCartItems_: MerchantCart[];
  private eventTracker_: EventTracker = new EventTracker();

  override connectedCallback() {
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
    this.shadowRoot!.querySelectorAll('.probe').forEach(
        el => this.intersectionObserver_!.observe(el));

    this.eventTracker_.add(
        this, 'discount-consent-accepted',
        () => this.onDiscountConsentAccepted_());
    this.eventTracker_.add(
        this, 'discount-consent-rejected',
        () => this.onDiscountConsentRejected_());
    this.eventTracker_.add(
        this, 'discount-consent-dismissed',
        () => this.onDiscountConsentDismissed_());
    this.eventTracker_.add(
        this, 'discount-consent-continued',
        () => this.onDiscountConsentContinued_());
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.intersectionObserver_!.disconnect();

    this.eventTracker_.removeAll();
  }

  private computeFirstThreeCartItems_(cartItems: MerchantCart[]):
      MerchantCart[] {
    return cartItems.slice(0, 3);
  }

  private getFaviconUrl_(url: string): string {
    const faviconUrl = new URL('chrome://favicon2/');
    faviconUrl.searchParams.set('size', '24');
    faviconUrl.searchParams.set('scale_factor', '1x');
    faviconUrl.searchParams.set('show_fallback_monogram', '');
    faviconUrl.searchParams.set('page_url', url);
    return faviconUrl.href;
  }

  private getImagesToShow_(imageUrls: string[]): string[] {
    return imageUrls.slice(0, 3);
  }

  private onCartMenuButtonClick_(e: DomRepeatEvent<MerchantCart>) {
    e.preventDefault();
    e.stopPropagation();
    this.currentMenuIndex_ = e.model.index;
    const merchant = this.cartItems[this.currentMenuIndex_].merchant;
    this.cartMenuHideItem_ =
        loadTimeData.getStringF('modulesCartCartMenuHideMerchant', merchant);
    this.cartMenuRemoveItem_ =
        loadTimeData.getStringF('modulesCartCartMenuRemoveMerchant', merchant);
    this.$.cartActionMenu.showAt(e.target as HTMLElement);
  }

  private async onCartHide_() {
    this.$.cartActionMenu.close();
    const merchant = this.cartItems[this.currentMenuIndex_].merchant;
    const cartUrl = this.cartItems[this.currentMenuIndex_].cartUrl;

    await ChromeCartProxy.getHandler().hideCart(cartUrl);

    this.dismissedCartData_ = {
      message: loadTimeData.getStringF(
          'modulesCartCartMenuHideMerchantToastMessage', merchant),
      restoreCallback: async () => {
        await ChromeCartProxy.getHandler().restoreHiddenCart(cartUrl);
      },
    };
    const isModuleVisible = await this.resetCartData_();
    if (isModuleVisible) {
      this.$.dismissCartToast.show();
    }
  }

  private async onCartRemove_() {
    this.$.cartActionMenu.close();
    const merchant = this.cartItems[this.currentMenuIndex_].merchant;
    const cartUrl = this.cartItems[this.currentMenuIndex_].cartUrl;

    await ChromeCartProxy.getHandler().removeCart(cartUrl);

    this.dismissedCartData_ = {
      message: loadTimeData.getStringF(
          'modulesCartCartMenuRemoveMerchantToastMessage', merchant),
      restoreCallback: async () => {
        await ChromeCartProxy.getHandler().restoreRemovedCart(cartUrl);
      },
    };
    const isModuleVisible = await this.resetCartData_();
    if (isModuleVisible) {
      this.$.dismissCartToast.show();
    }
  }

  private async onUndoDismissCartButtonClick_() {
    // Restore the module item.
    await this.dismissedCartData_!.restoreCallback();
    this.dismissedCartData_ = null;
    this.resetCartData_();

    // Notify the user.
    this.$.dismissCartToast.hide();
  }

  /** @return Whether the module is visible after reset. */
  private async resetCartData_(): Promise<boolean> {
    const {carts} = await ChromeCartProxy.getHandler().getMerchantCarts();
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
            await this.dismissedCartData_!.restoreCallback();
            this.dismissedCartData_ = null;
            const {carts} =
                await ChromeCartProxy.getHandler().getMerchantCarts();
            this.cartItems = carts;
          },
        },
      }));
      chrome.metricsPrivate.recordUserAction(
          'NewTabPage.Carts.DismissLastCartHidesModule');
    }
    return isModuleVisible;
  }

  private onInfoButtonClick_() {
    this.$.infoDialogRender.get().showModal();
  }

  private onDismissButtonClick_() {
    ChromeCartProxy.getHandler().hideCartModule();
    this.dispatchEvent(new CustomEvent('dismiss-module', {
      bubbles: true,
      composed: true,
      detail: {
        message:
            loadTimeData.getString('modulesCartModuleMenuHideToastMessage'),
        restoreCallback: () => {
          ChromeCartProxy.getHandler().restoreHiddenCartModule();
          chrome.metricsPrivate.recordUserAction(
              'NewTabPage.Carts.UndoHideModule');
        },
      },
    }));
    chrome.metricsPrivate.recordUserAction('NewTabPage.Carts.HideModule');
  }

  private onDisableButtonClick_() {
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
   */
  private onRightScrollClick_() {
    const carts = this.$.cartCarousel.querySelectorAll('.cart-container');
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
   */
  private onLeftScrollClick_() {
    const carts = this.$.cartCarousel.querySelectorAll('.cart-container');
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

  /** @param index The target index to scroll to. */
  private scrollToIndex_(index: number) {
    const carts =
        this.$.cartCarousel.querySelectorAll<HTMLElement>('.cart-container');
    // Calculate scroll shadow width as scroll offset.
    const leftScrollShadow =
        this.shadowRoot!.getElementById('leftScrollShadow');
    const rightScrollShadow =
        this.shadowRoot!.getElementById('rightScrollShadow');
    const scrollOffset = Math.max(
        leftScrollShadow ? leftScrollShadow.offsetWidth : 0,
        rightScrollShadow ? rightScrollShadow.offsetWidth : 0);
    let leftPosition = carts[index].offsetLeft - scrollOffset;
    // TODO(crbug.com/1198632): This could make a left scroll jump over cart
    // items.
    if (index === 0) {
      const consentCard = this.shadowRoot!.getElementById(
          this.discountConsentHasTwoSteps_ ? 'consentCardV2' : 'consentCard');
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

  /** @return Whether the item at index is completely visible. */
  private getVisibilityForIndex_(index: number): boolean {
    const cartCarousel = this.$.cartCarousel;
    const cart =
        cartCarousel.querySelectorAll<HTMLElement>('.cart-container')[index];
    return cart && (cart.offsetLeft > cartCarousel.scrollLeft) &&
        (cartCarousel.scrollLeft + cartCarousel.clientWidth) >
        (cart.offsetLeft + cart.offsetWidth);
  }

  private async onCartItemClick_(e: Event&{shouldNavigate?: boolean}) {
    const index =
        this.$.cartItemRepeat.indexForElement(e.target as HTMLElement)!;
    // When rule-based discount is enabled, clicking on the cart wouldn't
    // trigger navigation immediately. Instead, we'll fetch discount URL from
    // browser process and re-bind URL. Then, we create a new pointer event by
    // cloning the initial one so that we can re-trigger a navigation with the
    // new URL. This is to keep the navigation in render process for security
    // reasons.
    if (loadTimeData.getBoolean('ruleBasedDiscountEnabled') &&
        (e.shouldNavigate === undefined || e.shouldNavigate === false)) {
      e.preventDefault();
      const {discountUrl} = await ChromeCartProxy.getHandler().getDiscountURL(
          this.cartItems[index].cartUrl);
      this.set(`cartItems.${index}.cartUrl`, discountUrl);
      const cloneEvent = new PointerEvent(e.type, e);
      (cloneEvent as {shouldNavigate?: boolean}).shouldNavigate = true;
      this.$.cartCarousel.querySelectorAll('.cart-item')[index].dispatchEvent(
          cloneEvent);
      return;
    }
    ChromeCartProxy.getHandler().prepareForNavigation(
        this.cartItems[index].cartUrl, /*isNavigating=*/ true);
    this.dispatchEvent(new Event('usage', {bubbles: true, composed: true}));
    chrome.metricsPrivate.recordSmallCount('NewTabPage.Carts.ClickCart', index);
  }

  private onDiscountConsentRejected_() {
    this.showDiscountConsent = false;
    this.confirmDiscountConsentString_ =
        loadTimeData.getString('modulesCartDiscountConsentRejectConfirmation');
    this.$.confirmDiscountConsentToast.show();
    ChromeCartProxy.getHandler().onDiscountConsentAcknowledged(false);
    chrome.metricsPrivate.recordUserAction(
        'NewTabPage.Carts.RejectDiscountConsent');
  }

  private onDiscountConsentAccepted_() {
    this.showDiscountConsent = false;
    this.confirmDiscountConsentString_ =
        loadTimeData.getString('modulesCartDiscountConsentAcceptConfirmation');
    this.$.confirmDiscountConsentToast.show();
    ChromeCartProxy.getHandler().onDiscountConsentAcknowledged(true);
    chrome.metricsPrivate.recordUserAction(
        'NewTabPage.Carts.AcceptDiscountConsent');
  }

  private onDiscountConsentDismissed_() {
    this.showDiscountConsent = false;
    ChromeCartProxy.getHandler().onDiscountConsentDismissed();
    chrome.metricsPrivate.recordUserAction(
        'NewTabPage.Carts.DismissDiscountConsent');
  }

  private onDiscountConsentContinued_() {
    ChromeCartProxy.getHandler().onDiscountConsentContinued();
  }

  private onConfirmDiscountConsentClick_() {
    this.$.confirmDiscountConsentToast.hide();
  }

  private onCartItemContextMenuClick_(e: DomRepeatEvent<MerchantCart>) {
    const index = e.model.index;
    ChromeCartProxy.getHandler().prepareForNavigation(
        this.cartItems[index].cartUrl, /*isNavigating=*/ false);
  }
}

customElements.define(ChromeCartModuleElement.is, ChromeCartModuleElement);

async function createCartElement(): Promise<HTMLElement|null> {
  // getWarmWelcomeVisible makes server-side change and might flip the status of
  // whether welcome surface should show or not. Anything whose visibility
  // dependes on welcome surface (e.g. RBD consent) should check before
  // getWarmWelcomeVisible.
  const {consentVisible} =
      await ChromeCartProxy.getHandler().getDiscountConsentCardVisible();

  const {welcomeVisible} =
      await ChromeCartProxy.getHandler().getWarmWelcomeVisible();
  const {carts} = await ChromeCartProxy.getHandler().getMerchantCarts();
  chrome.metricsPrivate.recordSmallCount(
      'NewTabPage.Carts.CartCount', carts.length);

  if (carts.length === 0) {
    return null;
  }

  if (loadTimeData.getBoolean('ruleBasedDiscountEnabled')) {
    if (consentVisible) {
      recordOccurence('NewTabPage.Carts.DiscountConsentShow');
    }

    let discountedCartCount = 0;

    for (let i = 0; i < carts.length; i++) {
      const cart = carts[i];
      if (cart.discountText) {
        discountedCartCount++;
        chrome.metricsPrivate.recordSmallCount(
            'NewTabPage.Carts.DiscountAt', i);
      }
    }

    chrome.metricsPrivate.recordSmallCount(
        'NewTabPage.Carts.DiscountCountAtLoad', discountedCartCount);
  }

  const element = new ChromeCartModuleElement();
  if (welcomeVisible) {
    element.headerChipText = loadTimeData.getString('modulesNewTagLabel');
    element.headerDescriptionText =
        loadTimeData.getString('modulesCartWarmWelcome');
  }
  element.cartItems = carts;
  element.showDiscountConsent = consentVisible;
  return element;
}

export const chromeCartDescriptor: ModuleDescriptor = new ModuleDescriptor(
    /*id=*/ 'chrome_cart',
    /*name=*/ loadTimeData.getString('modulesCartSentence'), createCartElement);
