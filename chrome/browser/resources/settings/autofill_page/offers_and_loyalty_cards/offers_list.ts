// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-offers-list' is a read-only list of the user's
 * active Google Wallet offers. Each entry links out to Google Wallet.
 */
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '../../icons.html.js';
// <if expr="_google_chrome">
import '../../internal/icons.html.js';

// </if>

import {assert} from 'chrome://resources/js/assert.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../../i18n_setup.js';
import type {PersonalDataChangedListener} from '../autofill_manager_proxy.js';
import type {PaymentsManagerProxy} from '../payments/payments_manager_proxy.js';
import {PaymentsManagerImpl} from '../payments/payments_manager_proxy.js';

import {getCss} from './offers_list.css.js';
import {getHtml} from './offers_list.html.js';

type WalletDirectOfferEntry = chrome.autofillPrivate.WalletDirectOfferEntry;

export class SettingsOffersListElement extends CrLitElement {
  static get is() {
    return 'settings-offers-list';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      offers_: {type: Array},
    };
  }

  // <if expr="_google_chrome">
  protected readonly autofillEnableGradientGoogleLogos_: boolean =
      loadTimeData.getBoolean('autofillEnableGradientGoogleLogos');
  // </if>
  // In the order returned by `getWalletDirectOfferList()`, i.e. the most
  // recently issued offers first.
  protected accessor offers_: WalletDirectOfferEntry[] = [];

  private paymentsManager_: PaymentsManagerProxy =
      PaymentsManagerImpl.getInstance();
  private personalDataChangedListener_: PersonalDataChangedListener|null = null;

  override connectedCallback() {
    super.connectedCallback();

    // Offers are not part of the personal data changed event, but the event
    // fires whenever payments data, including offers, changes. Fetch the
    // offers again when it does.
    this.personalDataChangedListener_ = () => this.fetchOffers_();
    this.paymentsManager_.setPersonalDataManagerListener(
        this.personalDataChangedListener_);
    this.fetchOffers_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    if (this.personalDataChangedListener_) {
      this.paymentsManager_.removePersonalDataManagerListener(
          this.personalDataChangedListener_);
      this.personalDataChangedListener_ = null;
    }
  }

  private fetchOffers_() {
    this.paymentsManager_.getWalletDirectOfferList().then(
        (offers: WalletDirectOfferEntry[]) => {
          this.offers_ = offers;
        });
  }

  // TODO(crbug.com/546252995): Make the `WalletDirectOfferEntry` fields
  // required in autofill_private.idl, then mix in I18nMixinLit and replace
  // these two helpers with `i18n()` calls in the template.
  protected getPromoCodeLabel_(offer: WalletDirectOfferEntry): string {
    assert(offer.promoCode);
    return loadTimeData.getStringF('offersPromoCodeLabel', offer.promoCode);
  }

  protected getExpirationLabel_(offer: WalletDirectOfferEntry): string {
    assert(offer.expirationDate);
    return loadTimeData.getStringF(
        'offersExpirationLabel', offer.expirationDate);
  }

  // <if expr="_google_chrome">
  /**
   * Returns a srcset that scales `url` based on the screen resolution.
   */
  protected getScaledSrcSet_(url: string): string {
    return `${url} 1x, ${url}@2x 2x`;
  }
  // </if>

  protected onWalletLinkClick_(e: Event) {
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    const offer = this.offers_[index];
    assert(offer?.offerDetailsUrl);
    OpenWindowProxyImpl.getInstance().openUrl(offer.offerDetailsUrl);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-offers-list': SettingsOffersListElement;
  }
}

customElements.define(SettingsOffersListElement.is, SettingsOffersListElement);
