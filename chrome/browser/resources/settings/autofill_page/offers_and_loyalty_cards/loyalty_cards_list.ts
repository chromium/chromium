// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-loyalty-cards-list' is a read-only list of the
 * user's Google Wallet loyalty cards. Each entry links out to Google Wallet.
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
import type {LoyaltyCardsChangedListener, PaymentsManagerProxy} from '../payments/payments_manager_proxy.js';
import {PaymentsManagerImpl} from '../payments/payments_manager_proxy.js';

import {getCss} from './loyalty_cards_list.css.js';
import {getHtml} from './loyalty_cards_list.html.js';

type LoyaltyCardEntry = chrome.autofillPrivate.LoyaltyCardEntry;

// Compares strings in the user's locale, ignoring differences in capitalization
// and diacritics. Created once rather than on every comparison.
const LOYALTY_CARD_COLLATOR =
    new Intl.Collator(undefined, {sensitivity: 'base'});

export class SettingsLoyaltyCardsListElement extends CrLitElement {
  static get is() {
    return 'settings-loyalty-cards-list';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      // <if expr="_google_chrome">
      autofillEnableGradientGoogleLogos_: {type: Boolean},
      // </if>
      loyaltyCards_: {type: Array},
    };
  }

  // <if expr="_google_chrome">
  protected accessor autofillEnableGradientGoogleLogos_: boolean =
      loadTimeData.getBoolean('autofillEnableGradientGoogleLogos');
  // </if>
  protected accessor loyaltyCards_: LoyaltyCardEntry[] = [];

  private paymentsManager_: PaymentsManagerProxy =
      PaymentsManagerImpl.getInstance();
  private loyaltyCardsChangedListener_: LoyaltyCardsChangedListener|null = null;

  override connectedCallback() {
    super.connectedCallback();

    this.loyaltyCardsChangedListener_ = (loyaltyCards: LoyaltyCardEntry[]) => {
      this.loyaltyCards_ = loyaltyCards.sort(this.loyaltyCardsComparator_);
    };
    this.paymentsManager_.setLoyaltyCardsChangedListener(
        this.loyaltyCardsChangedListener_);
    this.paymentsManager_.getLoyaltyCardList().then(
        this.loyaltyCardsChangedListener_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    if (this.loyaltyCardsChangedListener_) {
      this.paymentsManager_.removeLoyaltyCardsChangedListener(
          this.loyaltyCardsChangedListener_);
      this.loyaltyCardsChangedListener_ = null;
    }
  }

  /**
   * Sorts loyalty cards by merchant name, then card number, using the user's
   * locale and without distinguishing between different capitalization or
   * diacritics.
   */
  private loyaltyCardsComparator_(a: LoyaltyCardEntry, b: LoyaltyCardEntry):
      number {
    return LOYALTY_CARD_COLLATOR.compare(a.merchantName, b.merchantName) ||
        LOYALTY_CARD_COLLATOR.compare(a.loyaltyCardNumber, b.loyaltyCardNumber);
  }

  /**
   * Returns a srcset that scales `url` based on the screen resolution.
   */
  protected getScaledSrcSet_(url: string): string {
    return `${url} 1x, ${url}@2x 2x`;
  }

  protected onWalletLinkClick_(e: Event) {
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    const loyaltyCard = this.loyaltyCards_[index];
    assert(loyaltyCard);
    OpenWindowProxyImpl.getInstance().openUrl(loyaltyCard.walletUrl);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-loyalty-cards-list': SettingsLoyaltyCardsListElement;
  }
}

customElements.define(
    SettingsLoyaltyCardsListElement.is, SettingsLoyaltyCardsListElement);
