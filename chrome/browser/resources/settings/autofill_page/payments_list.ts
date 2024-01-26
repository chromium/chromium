// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'payments-list' is a list of saved payment methods (credit
 * cards etc.) to be shown in the settings page.
 */

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../settings_shared.css.js';
import './credit_card_list_entry.js';
import './iban_list_entry.js';
import './passwords_shared.css.js';

import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import type {SettingsCreditCardListEntryElement} from './credit_card_list_entry.js';
import type {SettingsIbanListEntryElement} from './iban_list_entry.js';
import {getTemplate} from './payments_list.html.js';

export class SettingsPaymentsListElement extends PolymerElement {
  static get is() {
    return 'settings-payments-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * An array of all saved credit cards.
       */
      creditCards: Array,

      /**
       * An array of all saved IBANs.
       */
      ibans: Array,

      /**
       * True if displaying IBANs in settings is enabled.
       */
      enableIbans_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showIbansSettings');
        },
      },

      /**
       * True iff both credit cards and IBANs will be shown.
       */
      showCreditCardIbanSeparator_: {
        type: Boolean,
        value: false,
        computed: 'computeShowCreditCardIbanSeparator_(' +
            'creditCards, ibans, enableIbans_)',
      },

      /**
       * True iff any payment methods will be shown.
       */
      showAnyPaymentMethods_: {
        type: Boolean,
        value: false,
        computed: 'computeShowAnyPaymentMethods_(' +
            'creditCards, ibans, enableIbans_)',
      },
    };
  }

  creditCards: chrome.autofillPrivate.CreditCardEntry[];
  ibans: chrome.autofillPrivate.IbanEntry[];
  private enableIbans_: boolean;
  private showCreditCardIbanSeparator_: boolean;
  private showAnyPaymentMethods_: boolean;

  /**
   * Focuses the next most appropriate element after removing a specific
   * credit card. Returns `false` if it could not find such an element,
   * in this case the focus is supposed to be handled by someone else.
   */
  updateFocusBeforeCreditCardRemoval(cardIndex: number): boolean {
    // The focused element is to be reset only if the last element is deleted,
    // when the number of "dom-repeat" nodes changes and the focus get lost.
    if (cardIndex === this.creditCards.length - 1) {
      return this.updateFocusBeforeRemoval_(this.getCreditCardId_(cardIndex));
    } else {
      return true;
    }
  }

  /**
   * Focuses the next most appropriate element after removing a specific
   * iban. Returns `false` if it could not find such an element,
   * in this case the focus is supposed to be handled by someone else.
   */
  updateFocusBeforeIbanRemoval(ibanIndex: number): boolean {
    // The focused element is to be reset only if the last element is deleted,
    // when the number of "dom-repeat" nodes changes and the focus get lost.
    if (ibanIndex === this.ibans.length - 1) {
      return this.updateFocusBeforeRemoval_(this.getIbanId_(ibanIndex));
    } else {
      return true;
    }
  }

  /**
   * Handles focus resetting across all payment method lists. Returns `false`
   * only when the last payment method is removed, in other cases sets the focus
   * to either the next or previous payment method.
   */
  private updateFocusBeforeRemoval_(id: string): boolean {
    const paymentMethods =
        this.shadowRoot!.querySelectorAll<SettingsCreditCardListEntryElement|
                                          SettingsIbanListEntryElement>(
            '.payment-method');

    if (paymentMethods.length <= 1) {
      return false;
    }

    const index = [...paymentMethods].findIndex((element) => element.id === id);
    const isLastItem = index === paymentMethods.length - 1;
    const indexToFocus = index + (isLastItem ? -1 : +1);
    const menu = paymentMethods[indexToFocus].dotsMenu;
    if (menu) {
      focusWithoutInk(menu);
      return true;
    }

    return false;
  }

  private getCreditCardId_(index: number): string {
    return `card-${index}`;
  }

  private getIbanId_(index: number): string {
    return `iban-${index}`;
  }

  /**
   * @return Whether the list exists and has items.
   */
  private hasSome_(list: any[]): boolean {
    return !!(list && list.length);
  }

  /**
   * @return true iff there are credit cards to be shown.
   */
  private showCreditCards_(): boolean {
    return this.hasSome_(this.creditCards);
  }

  /**
   * @return true iff there are IBANs to be shown.
   */
  private showIbans_(): boolean {
    return this.enableIbans_ && this.hasSome_(this.ibans);
  }

  /**
   * @return true iff both credit cards and IBANs will be shown.
   */
  private computeShowCreditCardIbanSeparator_(): boolean {
    return this.showCreditCards_() && this.showIbans_();
  }

  /**
   * @return true iff any payment methods will be shown.
   */
  private computeShowAnyPaymentMethods_(): boolean {
    return this.showCreditCards_() || this.showIbans_();
  }
}

customElements.define(
    SettingsPaymentsListElement.is, SettingsPaymentsListElement);
