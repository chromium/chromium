// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'credit-card-list-entry' is a credit card row to be shown in
 * the settings page.
 */

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../../i18n_setup.js';
import {CardBenefitsUserAction, MetricsBrowserProxyImpl} from '../../metrics_browser_proxy.js';

import {getCss} from './credit_card_list_entry.css.js';
import {getHtml} from './credit_card_list_entry.html.js';

/**
 * This function returns a string that can be used in a srcset to scale
 * the provided `url` based on the user's screen resolution.
 */
function getScaledSrcSet(url: string): string {
  return `${url} 1x, ${url}@2x 2x`;
}

const SettingsCreditCardListEntryElementBase = I18nMixinLit(CrLitElement);

export class SettingsCreditCardListEntryElement extends
    SettingsCreditCardListEntryElementBase {
  static get is() {
    return 'settings-credit-card-list-entry';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /** A saved credit card. */
      creditCard: {type: Object},

      autofillEnableWalletBrandingEnabled_: {type: Boolean},

      autofillEnableGradientGoogleLogosEnabled_: {type: Boolean},
    };
  }

  accessor creditCard: chrome.autofillPrivate.CreditCardEntry = {
    expirationMonth: '01',
    expirationYear: '2099',
    imageSrc: '',
    metadata: {
      isLocal: false,
      isVirtualCardEnrolled: false,
      isVirtualCardEnrollmentEligible: false,
      summaryLabel: '',
    },
  };

  protected accessor autofillEnableWalletBrandingEnabled_: boolean =
      loadTimeData.getBoolean('autofillEnableWalletBranding');

  protected accessor autofillEnableGradientGoogleLogosEnabled_: boolean =
      loadTimeData.getBoolean('autofillEnableGradientGoogleLogos');

  get dotsMenu(): HTMLElement|null {
    return this.shadowRoot.getElementById('creditCardMenu');
  }

  /**
   * Opens the credit card action menu.
   */
  protected onDotsMenuClick_() {
    this.fire('dots-card-menu-click', {
      creditCard: this.creditCard,
      anchorElement: this.shadowRoot.querySelector('#creditCardMenu'),
    });
  }

  protected onRemoteEditClick_() {
    this.fire('remote-card-menu-click', {
      creditCard: this.creditCard,
      anchorElement: this.shadowRoot.querySelector('#creditCardMenu'),
    });
  }

  protected onSummarySublabelTermsLinkClick_() {
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
  protected moreActionsTitle_(): string {
    const cardDescription = this.creditCard.nickname ||
        this.getCardNumberDescription_(this.creditCard) ||
        this.creditCard.name!;
    return this.i18n(
        this.creditCard.cvc ? 'moreActionsForCreditCardWithCvc' :
                              'moreActionsForCreditCard',
        cardDescription);
  }

  /**
   * The card has a product description or a nickname.
   */
  protected hasCardIdentifier_(): boolean {
    return (this.creditCard.metadata!.summarySublabel || '').length > 0;
  }

  /**
   * The 3-dot menu should be shown if the card is not a masked server card or
   * if the card is eligible for virtual card enrollment.
   */
  protected showDots_(): boolean {
    return this.creditCard.metadata!.isLocal ||
        this.isVirtualCardEnrollmentEligible_();
  }

  protected shouldShowOutlinkWithWalletBranding_(): boolean {
    return !this.showDots_() && this.autofillEnableWalletBrandingEnabled_;
  }

  protected shouldShowOutlinkWithoutWalletBranding_(): boolean {
    return !this.showDots_() && !this.autofillEnableWalletBrandingEnabled_;
  }

  private isVirtualCardEnrollmentEligible_(): boolean {
    return this.creditCard.metadata!.isVirtualCardEnrollmentEligible!;
  }

  private isVirtualCardEnrolled_(): boolean {
    return this.creditCard.metadata!.isVirtualCardEnrolled!;
  }

  protected getCardIdentifierAriaLabel_(): string {
    return this.creditCard.metadata!.summaryLabel || '';
  }

  protected getSummaryAriaLabel_(): string {
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
  protected getBenefitsTermsAriaLabel_(): string {
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
        this.creditCard.expirationYear.substring(2);
  }

  /**
   * Returns expiration date.
   */
  protected getExpirationlabel_(): string {
    return ' · ' + this.getCardExpiryDate_();
  }

  /**
   * Returns one of the following sublabels, based on the card's status:
   *   Virtual card enrollment tag
   *   'CVC saved' tag
   * e.g., one of the following:
   *   CVC saved
   *   Virtual card turned on
   *   Virtual card turned on | CVC saved
   */
  protected getSummarySublabel_(): string {
    const separator = ' | ';
    let summarySublabel =
        this.isVirtualCardEnrolled_() ? this.i18n('virtualCardTurnedOn') : '';
    if (this.isCardCvcAvailable_()) {
      if (summarySublabel.length > 0) {
        summarySublabel += separator;
      }
      summarySublabel += this.i18n('cvcTagForCreditCardListEntry');
    }
    return summarySublabel;
  }

  protected hasSummaryAndBenefitSublabel_(): boolean {
    return this.getSummarySublabel_().length > 0 &&
        this.isCardBenefitsProductUrlAvailable_();
  }

  protected getSummaryAriaSublabel_(): string {
    const expirationDate =
        this.i18n('creditCardExpDateA11yLabeled', this.getCardExpiryDate_());
    const sublabel = this.getSummarySublabel_().replace('|', ',');
    if (sublabel) {
      return `${expirationDate}, ${sublabel}`;
    }
    return expirationDate;
  }

  private shouldShowVirtualCardSecondarySublabel_(): boolean {
    return this.creditCard.metadata!.summarySublabel!.trim() !== '' ||
        this.isVirtualCardEnrolled_() ||
        this.isVirtualCardEnrollmentEligible_();
  }

  protected shouldShowPaymentsIndicator_(): boolean {
    return !this.creditCard.metadata!.isLocal;
  }

  private isCardCvcAvailable_(): boolean {
    return loadTimeData.getBoolean('cvcStorageAvailable') &&
        !!this.creditCard.cvc;
  }

  protected isCardBenefitsProductUrlAvailable_(): boolean {
    return !!this.creditCard.productTermsUrl;
  }

  protected getCardBenefitsProductUrl_(): string {
    return this.creditCard.productTermsUrl || '';
  }

  /**
   * When the provided `imageSrc` points toward a processor's default card art,
   * this function returns a string that will scale the image based on the
   * user's screen resolution, otherwise it will return the unmodified
   * `imageSrc`.
   */
  protected getCardImage_(): string {
    if (!this.creditCard.imageSrc) {
      return '';
    }
    return this.creditCard.imageSrc.startsWith('chrome://theme') ?
        getScaledSrcSet(this.creditCard.imageSrc) :
        this.creditCard.imageSrc;
  }

  protected getGooglePayLightModeLogoSrcSet_(): string {
    const logoId = this.autofillEnableGradientGoogleLogosEnabled_ ?
        'chrome://theme/IDR_AUTOFILL_GOOGLE_PAY_WITH_GRADIENT_SMALL' :
        'chrome://theme/IDR_AUTOFILL_GOOGLE_PAY_SMALL';
    return getScaledSrcSet(logoId);
  }

  protected getGooglePayDarkModeLogoSrcSet_(): string {
    const logoId = this.autofillEnableGradientGoogleLogosEnabled_ ?
        'chrome://theme/IDR_AUTOFILL_GOOGLE_PAY_WITH_GRADIENT_DARK_SMALL' :
        'chrome://theme/IDR_AUTOFILL_GOOGLE_PAY_DARK_SMALL';
    return getScaledSrcSet(logoId);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-credit-card-list-entry': SettingsCreditCardListEntryElement;
  }
}

customElements.define(
    SettingsCreditCardListEntryElement.is, SettingsCreditCardListEntryElement);
