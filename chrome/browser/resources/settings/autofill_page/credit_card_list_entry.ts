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
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {getTemplate} from './credit_card_list_entry.html.js';

const SettingsCreditCardListEntryElementBase = I18nMixin(PolymerElement);

class SettingsCreditCardListEntryElement extends
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

      /**
       * Whether virtual card metadata on settings page is enabled.
       */
      virtualCardMetadataEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('virtualCardMetadataEnabled');
        },
        readOnly: true,
      },
    };
  }

  creditCard: chrome.autofillPrivate.CreditCardEntry;
  private virtualCardEnrollmentEnabled_: boolean;
  private virtualCardMetadataEnabled_: boolean;

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

  /**
   * @returns the title for the More Actions button corresponding to the card
   *     which is described by the nickname or the network name and last 4
   *     digits or name
   */
  private moreActionsTitle_(creditCard: chrome.autofillPrivate.CreditCardEntry):
      string {
    if (creditCard.nickname) {
      return this.i18n('moreActionsForCreditCard', creditCard.nickname);
    }

    const cardNumber = creditCard.cardNumber;
    if (cardNumber) {
      const lastFourDigits =
          cardNumber.substring(Math.max(0, cardNumber.length - 4));
      if (lastFourDigits) {
        const network = creditCard.network || this.i18n('genericCreditCard');
        return this.i18n(
            'moreActionsForCreditCard',
            this.i18n(
                'moreActionsCreditCardDescription', network, lastFourDigits));
      }
    }

    return this.i18n('moreActionsForCreditCard', creditCard.name!);
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

  private isVirtualCardMetadataEnabled_(): boolean {
    return this.virtualCardMetadataEnabled_;
  }

  private shouldShowVirtualCardLabel_(): boolean {
    return this.isVirtualCardEnrolled_() &&
        !this.isVirtualCardMetadataEnabled_();
  }

  private shouldShowSecondarySublabel_(): boolean {
    return !!(this.creditCard.metadata!.summarySublabel!.trim() !== '' ||
              this.isVirtualCardEnrolled_() ||
              this.isVirtualCardEnrollmentEligible_()) &&
        this.isVirtualCardMetadataEnabled_();
  }

  private getSecondarySublabel_(): string {
    if (this.isVirtualCardEnrolled_()) {
      return this.i18n('virtualCardTurnedOn');
    }
    if (this.isVirtualCardEnrollmentEligible_()) {
      return this.i18n('virtualCardAvailable');
    }
    return this.creditCard.metadata!.summarySublabel!;
  }

  private shouldShowPaymentsLabel_(): boolean {
    return !this.creditCard.metadata!.isLocal &&
        !this.isVirtualCardMetadataEnabled_();
  }

  private shouldShowPaymentsIndicator_(): boolean {
    return !this.creditCard.metadata!.isLocal &&
        this.isVirtualCardMetadataEnabled_();
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
