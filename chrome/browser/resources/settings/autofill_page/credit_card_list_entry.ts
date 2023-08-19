// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'credit-card-list-entry' is a credit card row to be shown in
 * the settings page.
 */

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../i18n_setup.js';
import '../settings_shared.css.js';
import './passwords_shared.css.js';

import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {getTemplate} from './credit_card_list_entry.html.js';

const enum CardSummarySublabelType {
  VIRTUAL_CARD,
  EXPIRATION_DATE,
}

const SettingsCreditCardListEntryElementBase = I18nMixin(PolymerElement);

export class SettingsCreditCardListEntryElement extends
    SettingsCreditCardListEntryElementBase {
  static get is() {
    return 'settings-credit-card-list-entry';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** A saved credit card. */
      creditCard: Object,

      /**
       * Whether virtual card enrollment management on settings page is enabled.
       */
      virtualCardEnrollmentEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('virtualCardEnrollmentEnabled');
        },
        readOnly: true,
      },
    };
  }

  creditCard: chrome.autofillPrivate.CreditCardEntry;
  private readonly virtualCardEnrollmentEnabled_: boolean;

  get dotsMenu(): HTMLElement|null {
    return this.shadowRoot!.getElementById('creditCardMenu');
  }

  /**
   * Opens the credit card action menu.
   */
  private onDotsMenuClick_() {
    this.dispatchEvent(new CustomEvent('dots-card-menu-click', {
      bubbles: true,
      composed: true,
      detail: {
        creditCard: this.creditCard,
        anchorElement: this.shadowRoot!.querySelector('#creditCardMenu'),
      },
    }));
  }

  private onRemoteEditClick_() {
    this.dispatchEvent(new CustomEvent('remote-card-menu-click', {
      bubbles: true,
      composed: true,
    }));
  }

  private getCardNumberDescription_(
      creditCard: chrome.autofillPrivate.CreditCardEntry): string|undefined {
    const cardNumber = creditCard.cardNumber;
    if (cardNumber) {
      const lastFourDigits =
          cardNumber.substring(Math.max(0, cardNumber.length - 4));
      if (lastFourDigits) {
        const network = creditCard.network || this.i18n('genericCreditCard');
        return this.i18n('creditCardDescription', network, lastFourDigits);
      }
    }
    return undefined;
  }

  /**
   * @returns the title for the More Actions button corresponding to the card
   *     which is described by the nickname or the network name and last 4
   *     digits or name
   */
  private moreActionsTitle_(): string {
    const cardDescription = this.creditCard.nickname ||
        this.getCardNumberDescription_(this.creditCard) ||
        this.creditCard.name!;
    return this.i18n('moreActionsForCreditCard', cardDescription);
  }

  /**
   * The 3-dot menu should be shown if the card is not a masked server card or
   * if the card is eligble for virtual card enrollment.
   */
  private showDots_(): boolean {
    return !!(
        this.creditCard.metadata!.isLocal ||
        this.creditCard.metadata!.isCached ||
        this.isVirtualCardEnrollmentEligible_());
  }

  private isVirtualCardEnrollmentEligible_(): boolean {
    return this.virtualCardEnrollmentEnabled_ &&
        this.creditCard.metadata!.isVirtualCardEnrollmentEligible!;
  }

  private isVirtualCardEnrolled_(): boolean {
    return this.virtualCardEnrollmentEnabled_ &&
        this.creditCard.metadata!.isVirtualCardEnrolled!;
  }

  private getSummaryAriaLabel_(): string {
    const cardNumberDescription =
        this.getCardNumberDescription_(this.creditCard);
    if (cardNumberDescription) {
      return this.i18n('creditCardA11yLabeled', cardNumberDescription);
    }
    return this.creditCard.metadata!.summaryLabel;
  }

  private getCardSublabelType(): CardSummarySublabelType {
    return this.isVirtualCardEnrolled_() ?
        CardSummarySublabelType.VIRTUAL_CARD :
        CardSummarySublabelType.EXPIRATION_DATE;
  }

  /**
   * Returns virtual card metadata if the card is eligible for enrollment or has
   * already enrolled, or expiration date (MM/YY) otherwise.
   * E.g., 11/23, or Virtual card turned on
   */
  private getSummarySublabel_(): string {
    switch (this.getCardSublabelType()) {
      case CardSummarySublabelType.VIRTUAL_CARD:
        return this.i18n('virtualCardTurnedOn');
      case CardSummarySublabelType.EXPIRATION_DATE:
        assert(this.creditCard.expirationMonth);
        assert(this.creditCard.expirationYear);
        // Convert string (e.g. '06') to number (e.g. 6).
        return this.creditCard.expirationMonth + '/' +
            this.creditCard.expirationYear.toString().substring(2);
      default:
        assertNotReached();
    }
  }

  private getSummaryAriaSublabel_(): string {
    switch (this.getCardSublabelType()) {
      case CardSummarySublabelType.VIRTUAL_CARD:
        return this.getSummarySublabel_();
      case CardSummarySublabelType.EXPIRATION_DATE:
        return this.i18n(
            'creditCardExpDateA11yLabeled', this.getSummarySublabel_());
      default:
        assertNotReached();
    }
  }

  private shouldShowVirtualCardSecondarySublabel_(): boolean {
    return this.creditCard.metadata!.summarySublabel!.trim() !== '' ||
        this.isVirtualCardEnrolled_() ||
        this.isVirtualCardEnrollmentEligible_();
  }

  private shouldShowPaymentsIndicator_(): boolean {
    return !this.creditCard.metadata!.isLocal;
  }

  private getPaymentsLabel_(): string {
    if (this.creditCard.metadata!.isCached) {
      return this.i18n('googlePaymentsCached');
    }
    return this.i18n('googlePayments');
  }
}

customElements.define(
    SettingsCreditCardListEntryElement.is, SettingsCreditCardListEntryElement);
