// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'credit-card-list-entry' is a credit card row to be shown in
 * the settings page.
 */

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../settings_shared.css.js';
import './passwords_shared.css.js';
import './screen_reader_only.css.js';

import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {CardBenefitsUserAction, MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';

import {getTemplate} from './credit_card_list_entry.html.js';

const enum CardSummarySublabelType {
  EXPIRATION_DATE,
  EXPIRATION_DATE_WITH_BENEFITS_TAG,
  EXPIRATION_DATE_WITH_CVC_TAG,
  EXPIRATION_DATE_WITH_CVC_AND_BENEFITS_TAG,
  VIRTUAL_CARD,
  VIRTUAL_CARD_WITH_BENEFITS_TAG,
  VIRTUAL_CARD_WITH_CVC_TAG,
  VIRTUAL_CARD_WITH_CVC_AND_BENEFITS_TAG,
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
    };
  }

  creditCard: chrome.autofillPrivate.CreditCardEntry;

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
      detail: {
        creditCard: this.creditCard,
        anchorElement: this.shadowRoot!.querySelector('#creditCardMenu'),
      },
    }));
  }

  private onSummarySublabelTermsLinkClick_() {
    // Log the metric for user clicking on the card benefits terms hyperlink.
    MetricsBrowserProxyImpl.getInstance().recordAction(
        CardBenefitsUserAction.CARD_BENEFITS_TERMS_LINK_CLICKED);
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
   *     digits or name. If a card has CVC saved, there will be additional
   *     description to notify of the same.
   */
  private moreActionsTitle_(): string {
    const cardDescription = this.creditCard.nickname ||
        this.getCardNumberDescription_(this.creditCard) ||
        this.creditCard.name!;
    return this.i18n(
        this.creditCard.cvc ? 'moreActionsForCreditCardWithCvc' :
                              'moreActionsForCreditCard',
        cardDescription);
  }

  /**
   * The 3-dot menu should be shown if the card is not a masked server card or
   * if the card is eligible for virtual card enrollment.
   */
  private showDots_(): boolean {
    return !!(
        this.creditCard.metadata!.isLocal ||
        this.isVirtualCardEnrollmentEligible_());
  }

  private isVirtualCardEnrollmentEligible_(): boolean {
    return this.creditCard.metadata!.isVirtualCardEnrollmentEligible!;
  }

  private isVirtualCardEnrolled_(): boolean {
    return this.creditCard.metadata!.isVirtualCardEnrolled!;
  }

  private getSummaryAriaLabel_(): string {
    const cardNumberDescription =
        this.getCardNumberDescription_(this.creditCard);
    if (cardNumberDescription) {
      return this.i18n('creditCardA11yLabeled', cardNumberDescription);
    }
    return this.creditCard.metadata!.summaryLabel;
  }

  /**
   * Returns an aria label for the benefits terms link such as "See terms for
   * Amex ending in 0001". If no card description is available, then the
   * default text such as "See terms here" is returned.
   */
  private getBenefitsTermsAriaLabel_(): string {
    const cardNumberDescription =
        this.getCardNumberDescription_(this.creditCard);
    if (cardNumberDescription) {
      return this.i18n('benefitsTermsAriaLabel', cardNumberDescription);
    }
    return this.i18n('benefitsTermsTagForCreditCardListEntry');
  }

  private getCardExpiryDate_(): string {
    assert(this.creditCard.expirationMonth);
    assert(this.creditCard.expirationYear);
    // Truncate the year down to two digits (eg. 2023 to 23).
    return this.creditCard.expirationMonth + '/' +
        this.creditCard.expirationYear.toString().substring(2);
  }

  private getCardSublabelType(): CardSummarySublabelType {
    if (this.isVirtualCardEnrolled_()) {
      if (this.isCardCvcAvailable_()) {
        return this.isCardBenefitsProductUrlAvailable_() ?
            CardSummarySublabelType.VIRTUAL_CARD_WITH_CVC_AND_BENEFITS_TAG :
            CardSummarySublabelType.VIRTUAL_CARD_WITH_CVC_TAG;
      }
      return this.isCardBenefitsProductUrlAvailable_() ?
          CardSummarySublabelType.VIRTUAL_CARD_WITH_BENEFITS_TAG :
          CardSummarySublabelType.VIRTUAL_CARD;
    }
    if (this.isCardCvcAvailable_()) {
      return this.isCardBenefitsProductUrlAvailable_() ?
          CardSummarySublabelType.EXPIRATION_DATE_WITH_CVC_AND_BENEFITS_TAG :
          CardSummarySublabelType.EXPIRATION_DATE_WITH_CVC_TAG;
    }
    return this.isCardBenefitsProductUrlAvailable_() ?
        CardSummarySublabelType.EXPIRATION_DATE_WITH_BENEFITS_TAG :
        CardSummarySublabelType.EXPIRATION_DATE;
  }

  /**
   * Returns one of the following sublabels, based on the card's status:
   *   Virtual card enrollment tag
   *   Expiration date tag (MM/YY)
   *   'CVC saved' tag
   *   Benefit tag (Place the benefit tag last because it includes a link to
   *                product terms.)
   * e.g., one of the following:
   *   11/23
   *   11/23 | CVC saved
   *   11/23 | Card benefits available (terms apply)
   *   11/23 | CVC saved | Card benefits available (terms apply)
   *   Virtual card turned on
   *   Virtual card turned on | CVC saved
   *   Virtual card turned on | Card benefits available (terms apply)
   *   Virtual card turned on | CVC saved | Card benefits available (terms
   *     apply)
   */
  private getSummarySublabel_(): string {
    const separator = ' | ';
    let summarySublabel = this.isVirtualCardEnrolled_() ?
        this.i18n('virtualCardTurnedOn') :
        this.getCardExpiryDate_();
    if (this.isCardCvcAvailable_()) {
      summarySublabel += separator + this.i18n('cvcTagForCreditCardListEntry');
    }
    return summarySublabel;
  }

  private getSummaryAriaSublabel_(): string {
    switch (this.getCardSublabelType()) {
      case CardSummarySublabelType.VIRTUAL_CARD_WITH_CVC_AND_BENEFITS_TAG:
      case CardSummarySublabelType.VIRTUAL_CARD_WITH_BENEFITS_TAG:
      case CardSummarySublabelType.VIRTUAL_CARD_WITH_CVC_TAG:
      case CardSummarySublabelType.VIRTUAL_CARD:
        return this.getSummarySublabel_();
      case CardSummarySublabelType.EXPIRATION_DATE_WITH_CVC_AND_BENEFITS_TAG:
      case CardSummarySublabelType.EXPIRATION_DATE_WITH_BENEFITS_TAG:
      case CardSummarySublabelType.EXPIRATION_DATE_WITH_CVC_TAG:
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

  private isCardCvcAvailable_(): boolean {
    return loadTimeData.getBoolean('cvcStorageAvailable') &&
        !!this.creditCard.cvc;
  }

  private isCardBenefitsProductUrlAvailable_(): boolean {
    return !!this.creditCard.productTermsUrl;
  }

  private getCardBenefitsProductUrl_(): string {
    return this.creditCard.productTermsUrl || '';
  }

  /**
   * When the provided `imageSrc` points toward a processor's default card art,
   * this function returns a string that will scale the image based on the
   * user's screen resolution, otherwise it will return the unmodified
   * `imageSrc`.
   */
  private getCardImage_(imageSrc: string): string {
    return imageSrc.startsWith('chrome://theme') ?
        this.getScaledSrcSet_(imageSrc) :
        imageSrc;
  }

  /**
   * This function returns a string that can be used in a srcset to scale
   * the provided `url` based on the user's screen resolution.
   */
  private getScaledSrcSet_(url: string): string {
    return `${url} 1x, ${url}@2x 2x`;
  }
}

customElements.define(
    SettingsCreditCardListEntryElement.is, SettingsCreditCardListEntryElement);
